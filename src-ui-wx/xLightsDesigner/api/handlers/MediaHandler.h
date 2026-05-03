#pragma once

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
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

    [[nodiscard]] transport::ApiResponse handleSetShowDirectory(const transport::ApiRequest& request) const {
        return handleShowDirectoryMutation(request, false);
    }

    [[nodiscard]] transport::ApiResponse handleRequestShowDirectoryAccess(const transport::ApiRequest& request) const {
        return handleShowDirectoryMutation(request, true);
    }

private:
    [[nodiscard]] transport::ApiResponse handleShowDirectoryMutation(const transport::ApiRequest& request, bool requestAccess) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::MediaShowDirectoryRequest showRequest;
        showRequest.showDirectory = parsing::ReadString(request.params, "showDirectory");
        if (showRequest.showDirectory.empty()) {
            showRequest.showDirectory = parsing::ReadString(request.params, "path");
        }
        showRequest.force = parsing::ReadBool(request.params, "force", false);
        showRequest.permanent = parsing::ReadBool(request.params, "permanent", false);

        if (showRequest.showDirectory.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"VALIDATION_ERROR", "showDirectory is required.", nlohmann::json::object()};
            return response;
        }

        if (!IsDesignerApiStartupSettled()) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                "APP_NOT_READY",
                "xLightsDesigner media.showDirectory is blocked until xLights startup has fully settled.",
                nlohmann::json{{"showDirectory", showRequest.showDirectory}, {"retryAfterMs", GetDesignerApiStartupSettleRemainingMs()}}
            };
            return response;
        }

        const auto result = requestAccess
            ? _service.requestShowDirectoryAccess(showRequest)
            : _service.setShowDirectory(showRequest);
        if (result.errorCode.has_value()) {
            const auto code = result.errorCode.value();
            response.statusCode = (code == "VALIDATION_ERROR") ? 400 :
                (code == "SHOW_DIRECTORY_ACCESS_DENIED") ? 403 :
                (code == "SHOW_DIRECTORY_ACCESS_CANCELLED" || code == "SHOW_DIRECTORY_ACCESS_MISMATCH") ? 409 :
                (code == "SHOW_DIRECTORY_NOT_FOUND") ? 404 :
                (code == "SEQUENCE_OPEN" || code == "UNSAVED_CHANGES") ? 409 :
                500;
            response.error = transport::ApiError{
                code,
                result.errorMessage.value_or("Unable to switch show directory."),
                nlohmann::json{
                    {"showDirectory", showRequest.showDirectory},
                    {"previousShowDirectory", result.previousShowDirectory.value_or("")}
                }
            };
            return response;
        }

        response.data["changed"] = result.changed;
        response.data["sequenceClosed"] = result.sequenceClosed;
        response.data["previousShowDirectory"] = result.previousShowDirectory.value_or("");
        response.data["showDirectory"] = result.showDirectory.value_or("");
        return response;
    }

    services::MediaService _service;
};

} // namespace xLightsDesigner::api::handlers
