#pragma once

#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace xLightsDesigner::api::transport {

struct ApiError {
    std::string code;
    std::string message;
    nlohmann::json details = nlohmann::json::object();
};

struct ApiResponse {
    int statusCode = 200;
    std::string command;
    std::string requestId;
    nlohmann::json data = nlohmann::json::object();
    nlohmann::json warnings = nlohmann::json::array();
    std::optional<ApiError> error;

    [[nodiscard]] bool ok() const {
        return !error.has_value();
    }

    [[nodiscard]] nlohmann::json toJson() const {
        nlohmann::json json;
        json["statusCode"] = statusCode;
        if (!command.empty()) {
            json["command"] = command;
        }
        if (!requestId.empty()) {
            json["requestId"] = requestId;
        }
        if (ok()) {
            json["data"] = data;
            if (!warnings.empty()) {
                json["warnings"] = warnings;
            }
        } else {
            json["error"] = {
                {"code", error->code},
                {"message", error->message},
                {"details", error->details}
            };
        }
        return json;
    }
};

} // namespace xLightsDesigner::api::transport
