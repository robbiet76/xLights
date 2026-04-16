#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <log.h>
#include <nlohmann/json.hpp>

#include "DesignerApiListener.h"
#include "api/transport/ApiRequest.h"
#include "api/transport/ApiResponse.h"

namespace xLightsDesigner {

struct DesignerApiHealthSnapshot {
    bool listenerConfigured = false;
    bool listenerRunning = false;
    bool listenerReachable = false;
    int listenerPort = 0;
    std::uint64_t listenerBindCount = 0;
    std::uint64_t listenerRestartCount = 0;
    std::string listenerLastBindAt;
    std::string listenerLastReachableAt;
    std::string listenerLastFailure;
    bool workerRunning = false;
    bool busy = false;
    std::size_t queueDepth = 0;
    std::uint64_t totalSubmitted = 0;
    std::uint64_t totalCompleted = 0;
    std::uint64_t totalFailed = 0;
    std::string activeJobId;
    bool appReady = false;
    bool startupSettled = false;
    std::uint64_t settleWindowMs = 0;
    std::uint64_t settleRemainingMs = 0;
    std::string initializedAt;
    std::string appReadyAt;
    std::string startupState;
};

struct DesignerApiJobSnapshot {
    std::string jobId;
    std::string command;
    std::string requestId;
    std::string state;
    std::string createdAt;
    std::string startedAt;
    std::string finishedAt;
    std::optional<api::transport::ApiResponse> response;
};

namespace detail {
struct PendingDesignerApiJob {
    std::string jobId;
    std::function<api::transport::ApiResponse()> operation;
};

inline std::mutex& DesignerApiJobMutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::condition_variable& DesignerApiJobCv() {
    static std::condition_variable cv;
    return cv;
}

inline std::deque<PendingDesignerApiJob>& DesignerApiPendingJobs() {
    static std::deque<PendingDesignerApiJob> jobs;
    return jobs;
}

inline std::map<std::string, DesignerApiJobSnapshot>& DesignerApiJobs() {
    static std::map<std::string, DesignerApiJobSnapshot> jobs;
    return jobs;
}

inline std::thread& DesignerApiWorkerThread() {
    static std::thread worker;
    return worker;
}

inline bool& DesignerApiWorkerStopRequested() {
    static bool stopRequested = false;
    return stopRequested;
}

inline bool& DesignerApiWorkerRunningFlag() {
    static bool running = false;
    return running;
}

inline bool& DesignerApiWorkerBusyFlag() {
    static bool busy = false;
    return busy;
}

inline std::string& DesignerApiActiveJobId() {
    static std::string jobId;
    return jobId;
}

inline std::uint64_t& DesignerApiJobCounter() {
    static std::uint64_t counter = 0;
    return counter;
}

inline std::uint64_t& DesignerApiTotalSubmitted() {
    static std::uint64_t value = 0;
    return value;
}

inline std::uint64_t& DesignerApiTotalCompleted() {
    static std::uint64_t value = 0;
    return value;
}

inline std::uint64_t& DesignerApiTotalFailed() {
    static std::uint64_t value = 0;
    return value;
}

inline bool& DesignerApiAppReadyFlag() {
    static bool value = false;
    return value;
}

inline std::chrono::steady_clock::time_point& DesignerApiInitializedSteady() {
    static auto value = std::chrono::steady_clock::now();
    return value;
}

inline std::optional<std::chrono::steady_clock::time_point>& DesignerApiAppReadySteady() {
    static std::optional<std::chrono::steady_clock::time_point> value;
    return value;
}

inline std::string& DesignerApiInitializedAt() {
    static std::string value;
    return value;
}

inline std::string& DesignerApiAppReadyAt() {
    static std::string value;
    return value;
}

inline std::uint64_t ResolveDesignerStartupSettleMs() {
    const char* value = std::getenv("XLIGHTS_DESIGNER_STARTUP_SETTLE_MS");
    if (value == nullptr || *value == '\0') {
        return 60000;
    }
    return static_cast<std::uint64_t>(std::strtoull(value, nullptr, 10));
}

inline std::string RuntimeNowUtcIso8601() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t nowTime = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &nowTime);
#else
    gmtime_r(&nowTime, &tm);
#endif
    std::ostringstream buffer;
    buffer << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return buffer.str();
}

inline void AppendRuntimeTrace(const std::string& line) {
    std::ofstream trace("/tmp/xlights-open-sequence-trace.log", std::ios::app);
    trace << line << std::endl;
    std::cerr << line << std::endl;
}

inline void RunDesignerApiWorkerLoop() {
    while (true) {
        PendingDesignerApiJob job;
        {
            std::unique_lock<std::mutex> lock(DesignerApiJobMutex());
            DesignerApiJobCv().wait(lock, []() {
                return DesignerApiWorkerStopRequested() || !DesignerApiPendingJobs().empty();
            });
            if (DesignerApiWorkerStopRequested() && DesignerApiPendingJobs().empty()) {
                break;
            }
            job = std::move(DesignerApiPendingJobs().front());
            DesignerApiPendingJobs().pop_front();
            AppendRuntimeTrace(std::string("runtime dequeued job=") + job.jobId);
            DesignerApiWorkerBusyFlag() = true;
            DesignerApiActiveJobId() = job.jobId;
            auto found = DesignerApiJobs().find(job.jobId);
            if (found != DesignerApiJobs().end()) {
                found->second.state = "running";
                found->second.startedAt = RuntimeNowUtcIso8601();
            }
        }

        AppendRuntimeTrace(std::string("runtime before_invoke job=") + job.jobId);
        api::transport::ApiResponse response;
        try {
            response = job.operation();
            AppendRuntimeTrace(std::string("runtime after_invoke job=") + job.jobId + " status=" + std::to_string(response.statusCode));
        } catch (const std::exception& ex) {
            AppendRuntimeTrace(std::string("runtime exception job=") + job.jobId + " message=" + ex.what());
            response.statusCode = 500;
            response.command = "job.execute";
            response.error = api::transport::ApiError{"INTERNAL_ERROR", ex.what(), nlohmann::json::object()};
        } catch (...) {
            AppendRuntimeTrace(std::string("runtime unknown_exception job=") + job.jobId);
            response.statusCode = 500;
            response.command = "job.execute";
            response.error = api::transport::ApiError{"INTERNAL_ERROR", "Unknown job execution failure.", nlohmann::json::object()};
        }

        {
            std::lock_guard<std::mutex> lock(DesignerApiJobMutex());
            DesignerApiWorkerBusyFlag() = false;
            DesignerApiActiveJobId().clear();
            auto found = DesignerApiJobs().find(job.jobId);
            if (found != DesignerApiJobs().end()) {
                found->second.response = response;
                found->second.finishedAt = RuntimeNowUtcIso8601();
                if (response.ok()) {
                    found->second.state = "succeeded";
                    DesignerApiTotalCompleted()++;
                } else {
                    found->second.state = "failed";
                    DesignerApiTotalFailed()++;
                }
            }
        }

        spdlog::info("xLightsDesigner job {} finished.", job.jobId);
    }
}

} // namespace detail

inline void StartDesignerApiRuntime() {
    detail::AppendRuntimeTrace("runtime start_requested");
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    if (detail::DesignerApiWorkerRunningFlag()) {
        return;
    }
    detail::DesignerApiInitializedSteady() = std::chrono::steady_clock::now();
    detail::DesignerApiInitializedAt() = detail::RuntimeNowUtcIso8601();
    detail::DesignerApiAppReadyFlag() = false;
    detail::DesignerApiAppReadySteady().reset();
    detail::DesignerApiAppReadyAt().clear();
    detail::DesignerApiWorkerStopRequested() = false;
    detail::DesignerApiWorkerThread() = std::thread([]() { detail::RunDesignerApiWorkerLoop(); });
    detail::DesignerApiWorkerRunningFlag() = true;
    detail::AppendRuntimeTrace("runtime worker_started");
}

inline void MarkDesignerApiAppReady() {
    detail::AppendRuntimeTrace("runtime app_ready_notified");
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    detail::DesignerApiAppReadyFlag() = true;
    detail::DesignerApiAppReadySteady() = std::chrono::steady_clock::now();
    detail::DesignerApiAppReadyAt() = detail::RuntimeNowUtcIso8601();
}

inline bool IsDesignerApiStartupSettled() {
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    if (!detail::DesignerApiAppReadyFlag() || !detail::DesignerApiAppReadySteady().has_value()) {
        return false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - *detail::DesignerApiAppReadySteady()).count();
    return static_cast<std::uint64_t>(elapsed) >= detail::ResolveDesignerStartupSettleMs();
}

inline std::uint64_t GetDesignerApiStartupSettleRemainingMs() {
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    if (!detail::DesignerApiAppReadyFlag() || !detail::DesignerApiAppReadySteady().has_value()) {
        return detail::ResolveDesignerStartupSettleMs();
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - *detail::DesignerApiAppReadySteady()).count();
    const auto settle = detail::ResolveDesignerStartupSettleMs();
    if (elapsed < 0 || static_cast<std::uint64_t>(elapsed) >= settle) {
        return 0;
    }
    return settle - static_cast<std::uint64_t>(elapsed);
}

inline void StopDesignerApiRuntime() {
    {
        std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
        if (!detail::DesignerApiWorkerRunningFlag()) {
            return;
        }
        detail::DesignerApiWorkerStopRequested() = true;
    }
    detail::DesignerApiJobCv().notify_all();
    if (detail::DesignerApiWorkerThread().joinable()) {
        detail::DesignerApiWorkerThread().join();
    }
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    detail::DesignerApiWorkerRunningFlag() = false;
    detail::DesignerApiWorkerBusyFlag() = false;
    detail::DesignerApiActiveJobId().clear();
    detail::DesignerApiPendingJobs().clear();
}

inline std::string SubmitDesignerApiJob(
    const std::string& command,
    const std::string& requestId,
    const std::function<api::transport::ApiResponse()>& operation) {
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    const auto jobNumber = ++detail::DesignerApiJobCounter();
    const std::string jobId = "xld-job-" + std::to_string(jobNumber);
    detail::DesignerApiJobs()[jobId] = DesignerApiJobSnapshot{
        jobId,
        command,
        requestId,
        "queued",
        detail::RuntimeNowUtcIso8601(),
        std::string(),
        std::string(),
        std::nullopt
    };
    detail::DesignerApiPendingJobs().push_back({jobId, operation});
    detail::DesignerApiTotalSubmitted()++;
    detail::DesignerApiJobCv().notify_one();
    detail::AppendRuntimeTrace(std::string("runtime queued job=") + jobId + " command=" + command);
    spdlog::info("xLightsDesigner job {} queued for {}.", jobId, command);
    return jobId;
}

inline std::optional<DesignerApiJobSnapshot> GetDesignerApiJobSnapshot(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    const auto found = detail::DesignerApiJobs().find(jobId);
    if (found == detail::DesignerApiJobs().end()) {
        return std::nullopt;
    }
    return found->second;
}

inline DesignerApiHealthSnapshot GetDesignerApiHealthSnapshot() {
    const auto listener = GetDesignerApiListenerSnapshot();
    std::lock_guard<std::mutex> lock(detail::DesignerApiJobMutex());
    const auto settleWindowMs = detail::ResolveDesignerStartupSettleMs();
    std::uint64_t settleRemainingMs = settleWindowMs;
    bool startupSettled = false;
    if (detail::DesignerApiAppReadyFlag() && detail::DesignerApiAppReadySteady().has_value()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - *detail::DesignerApiAppReadySteady()).count();
        if (elapsed >= 0 && static_cast<std::uint64_t>(elapsed) >= settleWindowMs) {
            startupSettled = true;
            settleRemainingMs = 0;
        } else if (elapsed >= 0) {
            settleRemainingMs = settleWindowMs - static_cast<std::uint64_t>(elapsed);
        }
    }
    const std::string startupState = !detail::DesignerApiAppReadyFlag()
        ? "starting"
        : (startupSettled ? "ready" : "settling");
    return DesignerApiHealthSnapshot{
        listener.configured,
        listener.bound,
        listener.reachable,
        listener.port,
        listener.bindCount,
        listener.restartCount,
        listener.lastBindAt,
        listener.lastReachableAt,
        listener.lastFailure,
        detail::DesignerApiWorkerRunningFlag(),
        detail::DesignerApiWorkerBusyFlag(),
        detail::DesignerApiPendingJobs().size(),
        detail::DesignerApiTotalSubmitted(),
        detail::DesignerApiTotalCompleted(),
        detail::DesignerApiTotalFailed(),
        detail::DesignerApiActiveJobId(),
        detail::DesignerApiAppReadyFlag(),
        startupSettled,
        settleWindowMs,
        settleRemainingMs,
        detail::DesignerApiInitializedAt(),
        detail::DesignerApiAppReadyAt(),
        startupState
    };
}

inline api::transport::ApiResponse BuildQueuedJobAcceptedResponse(
    const api::transport::ApiRequest& request,
    const std::string& jobId) {
    api::transport::ApiResponse response;
    response.statusCode = 202;
    response.command = request.command;
    response.requestId = request.requestId;
    response.data["accepted"] = true;
    response.data["jobId"] = jobId;
    response.data["state"] = "queued";
    return response;
}

inline api::transport::ApiResponse BuildDesignerApiJobStatusResponse(
    const api::transport::ApiRequest& request,
    const DesignerApiJobSnapshot& snapshot) {
    api::transport::ApiResponse response;
    response.command = request.command;
    response.requestId = request.requestId;
    response.data["jobId"] = snapshot.jobId;
    response.data["command"] = snapshot.command;
    response.data["requestId"] = snapshot.requestId;
    response.data["state"] = snapshot.state;
    response.data["createdAt"] = snapshot.createdAt;
    response.data["startedAt"] = snapshot.startedAt;
    response.data["finishedAt"] = snapshot.finishedAt;
    if (snapshot.response.has_value()) {
        response.data["result"] = snapshot.response->toJson();
        response.data["result"]["ok"] = snapshot.response->ok();
    }
    return response;
}

} // namespace xLightsDesigner
