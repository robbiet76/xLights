#pragma once

#include "../services/MediaService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"

namespace xLightsDesigner::api::handlers {

class MediaHandler {
public:
    explicit MediaHandler(services::MediaService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetCurrent(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getCurrent();
        response.data["sequenceOpen"] = summary.sequenceOpen;
        response.data["sequencePath"] = summary.sequencePath.value_or("");
        response.data["mediaFile"] = summary.mediaFile.value_or("");
        response.data["showDirectory"] = summary.showDirectory.value_or("");
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetDirectories(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getDirectories();
        response.data["directories"] = nlohmann::json::array();
        for (const auto& directory : summary.directories) {
            response.data["directories"].push_back(directory);
        }
        return response;
    }

private:
    services::MediaService _service;
};

} // namespace xLightsDesigner::api::handlers
