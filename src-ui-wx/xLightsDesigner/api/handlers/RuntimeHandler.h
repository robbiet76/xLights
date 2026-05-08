#pragma once

#include <functional>
#include <utility>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/EffectMetadataStatus.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"

namespace xLightsDesigner::api::handlers {

class RuntimeHandler {
public:
    RuntimeHandler() = default;

    explicit RuntimeHandler(std::function<nlohmann::json()> modalStateProvider)
        : _modalStateProvider(std::move(modalStateProvider)) {}

    [[nodiscard]] transport::ApiResponse handleHealth(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto health = GetDesignerApiHealthSnapshot();
        response.data["listenerConfigured"] = health.listenerConfigured;
        response.data["listenerRunning"] = health.listenerRunning;
        response.data["listenerReachable"] = health.listenerReachable;
        response.data["listenerPort"] = health.listenerPort;
        response.data["listenerBindCount"] = health.listenerBindCount;
        response.data["listenerRestartCount"] = health.listenerRestartCount;
        response.data["listenerLastBindAt"] = health.listenerLastBindAt;
        response.data["listenerLastReachableAt"] = health.listenerLastReachableAt;
        response.data["listenerLastFailure"] = health.listenerLastFailure;
        response.data["workerRunning"] = health.workerRunning;
        response.data["busy"] = health.busy;
        response.data["queueDepth"] = health.queueDepth;
        response.data["activeJobId"] = health.activeJobId;
        response.data["totalSubmitted"] = health.totalSubmitted;
        response.data["totalCompleted"] = health.totalCompleted;
        response.data["totalFailed"] = health.totalFailed;
        response.data["appReady"] = health.appReady;
        response.data["startupSettled"] = health.startupSettled;
        response.data["settleWindowMs"] = health.settleWindowMs;
        response.data["settleRemainingMs"] = health.settleRemainingMs;
        response.data["initializedAt"] = health.initializedAt;
        response.data["appReadyAt"] = health.appReadyAt;
        response.data["startupState"] = health.startupState;
        if (_modalStateProvider) {
            response.data["modalState"] = _modalStateProvider();
        }
        response.data["state"] = !health.listenerReachable ? "degraded" : (health.busy ? "busy" : health.startupState);
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetJob(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto jobId = parsing::ReadString(request.params, "jobId");
        if (jobId.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"VALIDATION_ERROR", "jobs.get requires jobId.", nlohmann::json::object()};
            return response;
        }
        const auto snapshot = GetDesignerApiJobSnapshot(jobId);
        if (!snapshot.has_value()) {
            response.statusCode = 404;
            response.error = transport::ApiError{"NOT_FOUND", "Requested job was not found.", nlohmann::json{{"jobId", jobId}}};
            return response;
        }
        return BuildDesignerApiJobStatusResponse(request, *snapshot);
    }

    [[nodiscard]] transport::ApiResponse handleMetadataStatus(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto status = services::GetEffectMetadataStatus();
        response.data["metadataDir"] = status.metadataDir;
        response.data["metadataDirExists"] = status.metadataDirExists;
        response.data["schemaPresent"] = status.schemaPresent;
        response.data["schemaHash"] = status.schemaHash;
        response.data["effectFileCount"] = status.effectFileCount;
        response.data["sharedFileCount"] = status.sharedFileCount;
        response.data["bundleFingerprint"] = status.bundleFingerprint;
        response.data["files"] = status.files;
        return response;
    }

private:
    std::function<nlohmann::json()> _modalStateProvider;
};

} // namespace xLightsDesigner::api::handlers
