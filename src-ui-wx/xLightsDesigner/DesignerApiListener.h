#pragma once

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <log.h>
#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "api/transport/ApiResponse.h"
#include "api/transport/JsonTransport.h"
#include "DesignerDiagnostics.h"

namespace xLightsDesigner {

// Minimal HTTP endpoint used only by xLightsDesigner. It intentionally avoids a
// broader web framework so the xLights fork carries a small, auditable surface.
using DesignerEndpointHandler = std::function<std::optional<api::transport::ApiResponse>(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& queryParams,
    const nlohmann::json& body,
    const std::string& requestId)>;

struct DesignerApiListenerSnapshot {
    bool configured = false;
    bool bound = false;
    bool reachable = false;
    int port = 0;
    std::uint64_t bindCount = 0;
    std::uint64_t restartCount = 0;
    std::string lastBindAt;
    std::string lastReachableAt;
    std::string lastFailure;
};

namespace detail {
// Listener state is held in function-local statics to keep this header-only
// integration easy to include from the existing xLights build.
inline int& DesignerApiServerFd() {
    static int fd = -1;
    return fd;
}

inline DesignerEndpointHandler& DesignerApiHandler() {
    static DesignerEndpointHandler handler;
    return handler;
}

inline std::mutex& DesignerApiListenerMutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::thread& DesignerApiAcceptThread() {
    static std::thread thread;
    return thread;
}

inline std::thread& DesignerApiWatchdogThread() {
    static std::thread watchdog;
    return watchdog;
}

inline std::atomic<bool>& DesignerApiStopRequested() {
    static std::atomic<bool> stopRequested{false};
    return stopRequested;
}

inline std::atomic<bool>& DesignerApiAcceptRunning() {
    static std::atomic<bool> running{false};
    return running;
}

inline std::atomic<bool>& DesignerApiWatchdogStopRequested() {
    static std::atomic<bool> stopRequested{false};
    return stopRequested;
}

inline std::atomic<bool>& DesignerApiWatchdogRunning() {
    static std::atomic<bool> running{false};
    return running;
}

inline std::atomic<bool>& DesignerApiConfigured() {
    static std::atomic<bool> configured{false};
    return configured;
}

inline std::atomic<int>& DesignerApiConfiguredPort() {
    static std::atomic<int> port{0};
    return port;
}

inline std::uint64_t& DesignerApiBindCount() {
    static std::uint64_t count = 0;
    return count;
}

inline std::uint64_t& DesignerApiRestartCount() {
    static std::uint64_t count = 0;
    return count;
}

inline std::string& DesignerApiLastBindAt() {
    static std::string value;
    return value;
}

inline std::string& DesignerApiLastReachableAt() {
    static std::string value;
    return value;
}

inline std::string& DesignerApiLastFailure() {
    static std::string value;
    return value;
}

inline std::string NowUtcIso8601() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t nowTime = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &nowTime);
#else
    gmtime_r(&nowTime, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buffer);
}

inline int ResolveDesignerApiPort() {
    const char* value = std::getenv("XLIGHTS_DESIGNER_PORT");
    if (value == nullptr || *value == '\0') {
        return 49915;
    }
    return std::atoi(value);
}

inline std::string DecodeQueryComponent(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (ch == '%' && i + 2 < value.size()) {
            const auto hex = value.substr(i + 1, 2);
            char* end = nullptr;
            const long code = std::strtol(hex.c_str(), &end, 16);
            if (end != nullptr && *end == '\0') {
                decoded.push_back(static_cast<char>(code));
                i += 2;
                continue;
            }
        }
        decoded.push_back(ch);
    }
    return decoded;
}

inline std::map<std::string, std::string> ParseQueryParams(const std::string& query) {
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start < query.size()) {
        const size_t amp = query.find('&', start);
        const std::string token = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        if (!token.empty()) {
            const size_t eq = token.find('=');
            if (eq == std::string::npos) {
                params[DecodeQueryComponent(token)] = "";
            } else {
                params[DecodeQueryComponent(token.substr(0, eq))] = DecodeQueryComponent(token.substr(eq + 1));
            }
        }
        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }
    return params;
}

inline nlohmann::json ParseJsonBody(const std::string& rawBody) {
    if (rawBody.empty()) {
        return nlohmann::json::object();
    }
    if (!nlohmann::json::accept(rawBody)) {
        return nlohmann::json();
    }
    auto parsed = nlohmann::json::parse(rawBody, nullptr, false);
    return parsed.is_discarded() ? nlohmann::json() : parsed;
}

inline bool ProbeDesignerApiPort(int port) {
    AppendDesignerDiagnostic(std::string("StartDesignerApiListenerLocked port=") + std::to_string(port));
    if (port <= 0) {
        return false;
    }
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    const timeval timeout{1, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    const bool ok = (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    ::close(fd);
    return ok;
}

inline void RecordDesignerApiFailure(const std::string& message) {
    std::lock_guard<std::mutex> lock(DesignerApiListenerMutex());
    DesignerApiLastFailure() = message;
}

struct ParsedHttpRequest {
    std::string method;
    std::string uri;
    std::string path;
    std::map<std::string, std::string> queryParams;
    std::string body;
};

inline bool SendAll(int fd, const std::string& payload) {
    size_t sent = 0;
    while (sent < payload.size()) {
        const ssize_t rc = ::send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

inline std::string HttpReasonPhrase(int statusCode) {
    switch (statusCode) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    default: return "OK";
    }
}

inline bool SendJsonResponse(int fd, int statusCode, const nlohmann::json& envelope) {
    const std::string body = envelope.dump();
    std::string response;
    response.reserve(body.size() + 256);
    response += "HTTP/1.1 ";
    response += std::to_string(statusCode);
    response += " ";
    response += HttpReasonPhrase(statusCode);
    response += "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Headers: Content-Type, X-Request-Id\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response += "Connection: close\r\n";
    response += "Content-Length: ";
    response += std::to_string(body.size());
    response += "\r\n\r\n";
    response += body;
    return SendAll(fd, response);
}

inline bool ReadHttpRequest(int fd, ParsedHttpRequest& parsed) {
    std::string raw;
    raw.reserve(8192);
    char buffer[4096];
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;
    bool contentLengthKnown = false;

    while (true) {
        const ssize_t rc = ::recv(fd, buffer, sizeof(buffer), 0);
        if (rc <= 0) {
            return false;
        }
        raw.append(buffer, static_cast<size_t>(rc));

        if (headerEnd == std::string::npos) {
            headerEnd = raw.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                const std::string headers = raw.substr(0, headerEnd);
                std::string headersLower = headers;
                std::transform(headersLower.begin(), headersLower.end(), headersLower.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                const std::string key = "content-length:";
                size_t pos = headersLower.find(key);
                if (pos != std::string::npos) {
                    pos += key.size();
                    while (pos < headersLower.size() && headersLower[pos] == ' ') {
                        ++pos;
                    }
                    size_t end = headersLower.find("\r\n", pos);
                    const std::string value = headersLower.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
                    contentLength = static_cast<size_t>(std::strtoull(value.c_str(), nullptr, 10));
                    contentLengthKnown = true;
                }
                if (!contentLengthKnown || raw.size() >= headerEnd + 4 + contentLength) {
                    break;
                }
            }
        } else if (!contentLengthKnown || raw.size() >= headerEnd + 4 + contentLength) {
            break;
        }

        if (raw.size() > 1024 * 1024 * 16) {
            return false;
        }
    }

    if (headerEnd == std::string::npos) {
        return false;
    }

    const std::string headerBlock = raw.substr(0, headerEnd);
    const size_t lineEnd = headerBlock.find("\r\n");
    const std::string requestLine = headerBlock.substr(0, lineEnd);
    const size_t firstSpace = requestLine.find(' ');
    const size_t secondSpace = requestLine.find(' ', firstSpace == std::string::npos ? firstSpace : firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
        return false;
    }

    parsed.method = requestLine.substr(0, firstSpace);
    parsed.uri = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const size_t queryPos = parsed.uri.find('?');
    parsed.path = queryPos == std::string::npos ? parsed.uri : parsed.uri.substr(0, queryPos);
    parsed.queryParams = queryPos == std::string::npos ? std::map<std::string, std::string>{} : ParseQueryParams(parsed.uri.substr(queryPos + 1));
    parsed.body = raw.substr(headerEnd + 4);
    if (contentLengthKnown && parsed.body.size() > contentLength) {
        parsed.body.resize(contentLength);
    }
    return true;
}

inline void HandleDesignerApiClient(int clientFd) {
    const timeval timeout{2, 0};
    ::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    auto closeClient = [clientFd]() {
        ::shutdown(clientFd, SHUT_RDWR);
        ::close(clientFd);
    };

    try {
        ParsedHttpRequest request;
        if (!ReadHttpRequest(clientFd, request)) {
            SendJsonResponse(clientFd, 400, nlohmann::json{{"ok", false}, {"error", {{"code", "BAD_REQUEST"}, {"message", "Bad request."}}}});
            closeClient();
            return;
        }

        auto& handler = DesignerApiHandler();
        if (!handler) {
            SendJsonResponse(clientFd, 503, nlohmann::json{{"ok", false}, {"error", {{"code", "LISTENER_UNAVAILABLE"}, {"message", "xLightsDesigner listener is not available."}}}});
            closeClient();
            return;
        }

        const auto parsedBody = ParseJsonBody(request.body);
        if (!request.body.empty() && parsedBody.is_discarded()) {
            SendJsonResponse(clientFd, 400, nlohmann::json{{"ok", false}, {"error", {{"code", "INVALID_JSON"}, {"message", "Request body must be valid JSON."}}}});
            closeClient();
            return;
        }

        const auto apiResponse = handler(request.method, request.path, request.queryParams, parsedBody, std::string());
        if (!apiResponse.has_value()) {
            SendJsonResponse(clientFd, 404, nlohmann::json{{"ok", false}, {"error", {{"code", "NOT_FOUND"}, {"message", "Not found."}}}});
            closeClient();
            return;
        }

        if (!SendJsonResponse(clientFd, apiResponse->statusCode, api::transport::BuildJsonHttpEnvelope(*apiResponse))) {
            spdlog::warn("xLightsDesigner listener did not send response because connection lost.");
            RecordDesignerApiFailure("connection_lost");
        }
    } catch (const std::exception& ex) {
        spdlog::error("xLightsDesigner listener request failed with exception: {}", ex.what());
        SendJsonResponse(clientFd, 500, nlohmann::json{{"ok", false}, {"error", {{"code", "UNHANDLED_EXCEPTION"}, {"message", ex.what()}}}});
        RecordDesignerApiFailure("unhandled_exception");
    } catch (...) {
        spdlog::error("xLightsDesigner listener request failed with unknown exception.");
        SendJsonResponse(clientFd, 500, nlohmann::json{{"ok", false}, {"error", {{"code", "UNHANDLED_EXCEPTION"}, {"message", "Unknown request failure."}}}});
        RecordDesignerApiFailure("unhandled_exception");
    }

    closeClient();
}

inline void RunDesignerApiAcceptLoop() {
    DesignerApiAcceptRunning().store(true);
    while (!DesignerApiStopRequested().load()) {
        const int serverFd = DesignerApiServerFd();
        if (serverFd < 0) {
            break;
        }
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = ::accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (DesignerApiStopRequested().load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        std::thread([](int fd) { HandleDesignerApiClient(fd); }, clientFd).detach();
    }
    DesignerApiAcceptRunning().store(false);
    spdlog::info("xLightsDesigner listener accept loop stopped.");
}

inline bool StartDesignerApiListenerLocked(const DesignerEndpointHandler& handler, bool restart = false) {
    if (DesignerApiServerFd() >= 0) {
        DesignerApiStopRequested().store(true);
        ::shutdown(DesignerApiServerFd(), SHUT_RDWR);
        ::close(DesignerApiServerFd());
        DesignerApiServerFd() = -1;
    }
    if (DesignerApiAcceptThread().joinable()) {
        DesignerApiAcceptThread().join();
    }

    const int port = ResolveDesignerApiPort();
    DesignerApiConfiguredPort() = port;
    AppendDesignerDiagnostic(std::string("StartDesignerApiListenerLocked port=") + std::to_string(port));
    if (port <= 0) {
        spdlog::info("xLightsDesigner listener disabled because port is {}.", port);
        DesignerApiConfigured() = false;
        return false;
    }

    const int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        spdlog::warn("xLightsDesigner listener could not create socket for {}.", port);
        DesignerApiHandler() = DesignerEndpointHandler();
        DesignerApiConfigured() = false;
        RecordDesignerApiFailure("socket_create_failed");
        AppendDesignerDiagnostic("StartDesignerApiListenerLocked socket_create_failed");
        return false;
    }

    const int reuse = 1;
    ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(serverFd, 64) != 0) {
        spdlog::warn("xLightsDesigner listener could not listen on {}.", port);
        ::close(serverFd);
        DesignerApiHandler() = DesignerEndpointHandler();
        DesignerApiConfigured() = false;
        RecordDesignerApiFailure("bind_failed");
        AppendDesignerDiagnostic("StartDesignerApiListenerLocked bind_failed");
        return false;
    }

    DesignerApiHandler() = handler;
    DesignerApiStopRequested().store(false);
    DesignerApiServerFd() = serverFd;
    DesignerApiAcceptThread() = std::thread([]() { RunDesignerApiAcceptLoop(); });
    DesignerApiConfigured() = true;
    DesignerApiBindCount()++;
    if (restart) {
        DesignerApiRestartCount()++;
    }
    DesignerApiLastBindAt() = NowUtcIso8601();
    DesignerApiLastReachableAt() = DesignerApiLastBindAt();
    DesignerApiLastFailure().clear();
    AppendDesignerDiagnostic(std::string("StartDesignerApiListenerLocked started port=") + std::to_string(port));
    spdlog::info(
        "xLightsDesigner listener running on 0.0.0.0:{} (bind={} restart={}).",
        port,
        static_cast<unsigned long long>(DesignerApiBindCount()),
        static_cast<unsigned long long>(DesignerApiRestartCount()));
    return true;
}

inline void RunDesignerApiWatchdogLoop() {
    while (!DesignerApiWatchdogStopRequested().load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (DesignerApiWatchdogStopRequested().load()) {
            break;
        }
        DesignerEndpointHandler handler;
        int port = 0;
        {
            std::lock_guard<std::mutex> lock(DesignerApiListenerMutex());
            if (!DesignerApiConfigured().load()) {
                continue;
            }
            handler = DesignerApiHandler();
            port = DesignerApiConfiguredPort().load();
        }
        if (!handler || port <= 0) {
            continue;
        }
        if (ProbeDesignerApiPort(port)) {
            std::lock_guard<std::mutex> lock(DesignerApiListenerMutex());
            DesignerApiLastReachableAt() = NowUtcIso8601();
            continue;
        }
        spdlog::warn("xLightsDesigner listener watchdog detected unreachable port {}; restarting listener.", port);
        RecordDesignerApiFailure("watchdog_probe_failed");
        std::lock_guard<std::mutex> lock(DesignerApiListenerMutex());
        StartDesignerApiListenerLocked(handler, true);
    }
    spdlog::info("xLightsDesigner listener watchdog stopped.");
}

inline void EnsureDesignerApiWatchdogStarted() {
    if (DesignerApiWatchdogRunning().load()) {
        return;
    }
    DesignerApiWatchdogStopRequested().store(false);
    DesignerApiWatchdogThread() = std::thread([]() { RunDesignerApiWatchdogLoop(); });
    DesignerApiWatchdogRunning().store(true);
}

inline void StopDesignerApiWatchdog() {
    if (!DesignerApiWatchdogRunning().load()) {
        return;
    }
    DesignerApiWatchdogStopRequested().store(true);
    if (DesignerApiWatchdogThread().joinable()) {
        DesignerApiWatchdogThread().join();
    }
    DesignerApiWatchdogRunning().store(false);
}

} // namespace detail

// Starts the local listener and watchdog when XLD explicitly enables the API.
inline bool StartDesignerApiListener(const DesignerEndpointHandler& handler) {
    std::lock_guard<std::mutex> lock(detail::DesignerApiListenerMutex());
    const bool started = detail::StartDesignerApiListenerLocked(handler, false);
    if (started) {
        detail::EnsureDesignerApiWatchdogStarted();
    }
    return started;
}

inline bool IsDesignerApiListenerRunning() {
    std::lock_guard<std::mutex> lock(detail::DesignerApiListenerMutex());
    return detail::DesignerApiServerFd() >= 0 && detail::DesignerApiAcceptRunning().load();
}

inline DesignerApiListenerSnapshot GetDesignerApiListenerSnapshot() {
    std::lock_guard<std::mutex> lock(detail::DesignerApiListenerMutex());
    const int port = detail::DesignerApiConfiguredPort().load();
    return DesignerApiListenerSnapshot{
        detail::DesignerApiConfigured().load(),
        detail::DesignerApiServerFd() >= 0,
        detail::ProbeDesignerApiPort(port),
        port,
        detail::DesignerApiBindCount(),
        detail::DesignerApiRestartCount(),
        detail::DesignerApiLastBindAt(),
        detail::DesignerApiLastReachableAt(),
        detail::DesignerApiLastFailure()
    };
}

// Stops both the listener and watchdog before xLights exits.
inline void StopDesignerApiListener() {
    detail::StopDesignerApiWatchdog();
    std::lock_guard<std::mutex> lock(detail::DesignerApiListenerMutex());
    detail::DesignerApiStopRequested().store(true);
    if (detail::DesignerApiServerFd() >= 0) {
        ::shutdown(detail::DesignerApiServerFd(), SHUT_RDWR);
        ::close(detail::DesignerApiServerFd());
        detail::DesignerApiServerFd() = -1;
    }
    if (detail::DesignerApiAcceptThread().joinable()) {
        detail::DesignerApiAcceptThread().join();
    }
    detail::DesignerApiHandler() = DesignerEndpointHandler();
    detail::DesignerApiConfigured() = false;
    spdlog::info("xLightsDesigner listener stopped.");
}

} // namespace xLightsDesigner
