#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "ApiResponse.h"

namespace xLightsDesigner::api::transport {

// Flattens GET query params and POST JSON bodies into one string map. Complex
// JSON values remain serialized JSON so handlers can opt into structured parse.
inline std::map<std::string, std::string> MergeRequestParams(
    const std::map<std::string, std::string>& queryParams,
    const nlohmann::json& body) {
    std::map<std::string, std::string> params = queryParams;
    if (!body.is_object()) {
        return params;
    }

    for (auto it = body.begin(); it != body.end(); ++it) {
        if (it->is_string()) {
            params[it.key()] = it->get<std::string>();
        } else if (it->is_boolean()) {
            params[it.key()] = it->get<bool>() ? "true" : "false";
        } else if (it->is_number_integer()) {
            params[it.key()] = std::to_string(it->get<long long>());
        } else if (it->is_number_unsigned()) {
            params[it.key()] = std::to_string(it->get<unsigned long long>());
        } else if (it->is_number_float()) {
            params[it.key()] = std::to_string(it->get<double>());
        } else if (it->is_null()) {
            params[it.key()] = "";
        } else {
            params[it.key()] = it->dump();
        }
    }
    return params;
}

// Adds the public ok flag while preserving the handler's structured payload.
inline nlohmann::json BuildJsonHttpEnvelope(const ApiResponse& response) {
    nlohmann::json envelope = response.toJson();
    envelope["ok"] = response.ok();
    return envelope;
}

} // namespace xLightsDesigner::api::transport
