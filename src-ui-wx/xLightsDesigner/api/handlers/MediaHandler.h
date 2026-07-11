#pragma once

#include <utility>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/MediaService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"

namespace xLightsDesigner::api::handlers {

// Media endpoints cover current song/show-folder state and path validation.
// They are the only supported way for the app to switch xLights show folders.
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
        response.data["mediaContentFingerprint"] = summary.mediaContentFingerprint.value_or("");
        response.data["showDirectory"] = summary.showDirectory.value_or("");
        if (summary.durationMs.has_value()) response.data["durationMs"] = summary.durationMs.value();
        if (summary.sampleRate.has_value()) response.data["sampleRate"] = summary.sampleRate.value();
        if (summary.channelCount.has_value()) response.data["channelCount"] = summary.channelCount.value();
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

    [[nodiscard]] transport::ApiResponse handleValidatePathAccess(const transport::ApiRequest& request) const {
        // Batch validation lets the app preflight audio, sequence, image, and
        // generated-output paths before asking xLights to use them.
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::MediaPathAccessValidationRequest validationRequest;
        const auto pathsJsonText = parsing::ReadString(request.params, "paths");
        if (!pathsJsonText.empty()) {
            nlohmann::json pathsJson;
            try {
                pathsJson = nlohmann::json::parse(pathsJsonText);
            } catch (const std::exception& ex) {
                response.statusCode = 400;
                response.error = transport::ApiError{"VALIDATION_ERROR", "media.validatePathAccess received invalid paths JSON.", {{"reason", ex.what()}}};
                return response;
            }
            if (!pathsJson.is_array()) {
                response.statusCode = 400;
                response.error = transport::ApiError{"VALIDATION_ERROR", "media.validatePathAccess requires paths to be a JSON array.", nlohmann::json::object()};
                return response;
            }
            for (const auto& item : pathsJson) {
                if (!item.is_object() || !item.contains("path")) {
                    response.statusCode = 400;
                    response.error = transport::ApiError{"VALIDATION_ERROR", "Each media path check must include path.", {{"item", item}}};
                    return response;
                }
                validationRequest.checks.push_back({
                    item.value("kind", std::string()),
                    item.value("path", std::string()),
                    item.value("mustExist", true),
                    item.value("requireReadable", true),
                    item.value("requireWritable", false)
                });
            }
        } else {
            models::MediaPathAccessCheckRequest check;
            check.kind = parsing::ReadString(request.params, "kind");
            check.path = parsing::ReadString(request.params, "path");
            check.mustExist = parsing::ReadBool(request.params, "mustExist", true);
            check.requireReadable = parsing::ReadBool(request.params, "requireReadable", true);
            check.requireWritable = parsing::ReadBool(request.params, "requireWritable", false);
            if (!check.path.empty()) {
                validationRequest.checks.push_back(check);
            }
        }

        if (validationRequest.checks.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"VALIDATION_ERROR", "media.validatePathAccess requires at least one path check.", nlohmann::json::object()};
            return response;
        }

        const auto result = _service.validatePathAccess(validationRequest);
        response.data["ok"] = result.ok;
        response.data["checks"] = nlohmann::json::array();
        for (const auto& check : result.checks) {
            nlohmann::json warnings = nlohmann::json::array();
            for (const auto& warning : check.warnings) {
                warnings.push_back(warning);
            }
            response.data["checks"].push_back({
                {"kind", check.kind},
                {"path", check.path},
                {"exists", check.exists},
                {"readable", check.readable},
                {"writable", check.writable},
                {"accessible", check.accessible},
                {"trustedRoot", check.trustedRoot},
                {"withinCurrentShowDirectory", check.withinCurrentShowDirectory},
                {"errorCode", check.errorCode.value_or("")},
                {"errorMessage", check.errorMessage.value_or("")},
                {"warnings", warnings}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetAudioCapabilities(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getAudioCapabilities();
        response.data["sequenceOpen"] = summary.sequenceOpen;
        response.data["mediaAvailable"] = summary.mediaAvailable;
        response.data["mediaFile"] = summary.mediaFile.value_or("");
        response.data["capabilities"] = nlohmann::json::array();
        for (const auto& capability : summary.capabilities) {
            response.data["capabilities"].push_back({
                {"capabilityId", capability.capabilityId},
                {"available", capability.available},
                {"provider", capability.provider},
                {"reason", capability.reason}
            });
        }
        response.data["warnings"] = nlohmann::json::array();
        for (const auto& warning : summary.warnings) {
            response.data["warnings"].push_back(warning);
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleAnalyzeAudio(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.analyzeAudio();
        if (!summary.sequenceOpen) {
            response.statusCode = 409;
            response.error = transport::ApiError{"NO_SEQUENCE_OPEN", "Open a sequence with media before running xLights audio analysis.", nlohmann::json::object()};
            return response;
        }
        if (!summary.mediaAvailable) {
            response.statusCode = 409;
            response.error = transport::ApiError{"NO_MEDIA_AVAILABLE", "The current sequence does not have readable media for xLights audio analysis.", nlohmann::json::object()};
            return response;
        }

        response.data["sequenceOpen"] = summary.sequenceOpen;
        response.data["mediaAvailable"] = summary.mediaAvailable;
        response.data["sequencePath"] = summary.sequencePath.value_or("");
        response.data["mediaFile"] = summary.mediaFile.value_or("");
        response.data["mediaHash"] = summary.mediaHash.value_or("");
        if (summary.durationMs.has_value()) response.data["durationMs"] = summary.durationMs.value();
        if (summary.sampleRate.has_value()) response.data["sampleRate"] = summary.sampleRate.value();
        if (summary.channelCount.has_value()) response.data["channelCount"] = summary.channelCount.value();

        response.data["timingTracks"] = nlohmann::json::array();
        for (const auto& track : summary.timingTracks) {
            nlohmann::json marks = nlohmann::json::array();
            for (const auto& mark : track.marks) {
                nlohmann::json row = {
                    {"startMs", mark.startMs},
                    {"label", mark.label},
                    {"markType", mark.markType}
                };
                if (mark.endMs.has_value()) row["endMs"] = mark.endMs.value();
                if (mark.confidence.has_value()) row["confidence"] = mark.confidence.value();
                marks.push_back(row);
            }
            nlohmann::json row = {
                {"trackName", track.trackName},
                {"trackType", track.trackType},
                {"description", track.description},
                {"timingGranularity", track.timingGranularity},
                {"marks", marks}
            };
            if (track.confidence.has_value()) row["confidence"] = track.confidence.value();
            response.data["timingTracks"].push_back(row);
        }

        response.data["analysisEvidence"] = nlohmann::json::array();
        for (const auto& evidence : summary.evidence) {
            nlohmann::json row = {
                {"evidenceType", evidence.evidenceType},
                {"description", evidence.description},
                {"summary", evidence.summary}
            };
            if (evidence.confidence.has_value()) row["confidence"] = evidence.confidence.value();
            response.data["analysisEvidence"].push_back(row);
        }

        response.data["warnings"] = nlohmann::json::array();
        for (const auto& warning : summary.warnings) {
            response.data["warnings"].push_back(warning);
        }
        return response;
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
                (code == "SHOW_DIRECTORY_ACCESS_DENIED" || code == "SHOW_DIRECTORY_LAYOUT_UNREADABLE") ? 403 :
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
