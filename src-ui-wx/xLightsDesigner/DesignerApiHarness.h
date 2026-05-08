#pragma once

#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "api/transport/ApiResponse.h"

namespace xLightsDesigner {

std::optional<api::transport::ApiResponse> HandleDesignerApiRequest(
    const std::string& command,
    const std::map<std::string, std::string>& params,
    const std::string& requestId);

std::optional<nlohmann::json> HandleDesignerApiEndpointJson(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& queryParams,
    const nlohmann::json& body,
    const std::string& requestId);

class DesignerApiHarness {
public:
    [[nodiscard]] std::optional<nlohmann::json> invokeCommand(
        const std::string& command,
        const std::map<std::string, std::string>& params = {},
        const std::string& requestId = {}) const {
        auto response = HandleDesignerApiRequest(command, params, requestId);
        if (!response.has_value()) {
            return std::nullopt;
        }
        return response->toJson();
    }

    [[nodiscard]] std::optional<nlohmann::json> invokeEndpoint(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& queryParams = {},
        const nlohmann::json& body = nlohmann::json::object(),
        const std::string& requestId = {}) const {
        return HandleDesignerApiEndpointJson(method, path, queryParams, body, requestId);
    }
};

} // namespace xLightsDesigner
