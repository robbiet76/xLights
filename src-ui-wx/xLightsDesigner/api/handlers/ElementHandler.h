#pragma once

#include "../services/ElementService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

class ElementHandler {
public:
    explicit ElementHandler(services::ElementService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetSummary(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getSummary();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }

        response.data["elements"] = nlohmann::json::array();
        for (const auto& element : summary.elements) {
            nlohmann::json jsonElement = {
                {"name", element.name},
                {"type", element.type},
                {"totalEffectCount", element.totalEffectCount},
                {"layers", nlohmann::json::array()}
            };
            for (const auto& layer : element.layers) {
                jsonElement["layers"].push_back({
                    {"layerNumber", layer.layerNumber},
                    {"effectCount", layer.effectCount},
                    {"layerName", layer.layerName}
                });
            }
            response.data["elements"].push_back(std::move(jsonElement));
        }
        return response;
    }

private:
    services::ElementService _service;
};

} // namespace xLightsDesigner::api::handlers
