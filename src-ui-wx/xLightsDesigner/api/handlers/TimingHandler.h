#pragma once

#include <nlohmann/json.hpp>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/TimingService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

class TimingHandler {
public:
    explicit TimingHandler(services::TimingService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetTracks(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto summary = _service.getTracks();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        response.data["tracks"] = nlohmann::json::array();
        for (const auto& track : summary.tracks) {
            response.data["tracks"].push_back({{"name", track.name}, {"type", track.type}, {"markCount", track.markCount}});
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetMarks(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto trackName = parsing::ReadString(request.params, "track");
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (trackName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.getMarks requires track.", nlohmann::json::object()};
            return response;
        }
        if ((startMs >= 0 && endMs >= 0 && endMs < startMs) || startMs < -1 || endMs < -1) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.getMarks received an invalid time range.", {{"startMs", startMs}, {"endMs", endMs}}};
            return response;
        }
        models::TimingMarksRequest marksRequest;
        marksRequest.trackName = trackName;
        if (startMs >= 0) marksRequest.startMs = startMs;
        if (endMs >= 0) marksRequest.endMs = endMs;
        const auto summary = _service.getMarks(marksRequest);
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        if (!summary.trackFound) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested timing track was not found in the current sequence.", {{"track", trackName}}};
            return response;
        }
        response.data["track"] = summary.trackName;
        response.data["marks"] = nlohmann::json::array();
        for (const auto& mark : summary.marks) {
            response.data["marks"].push_back({{"startMs", mark.startMs}, {"endMs", mark.endMs}, {"label", mark.label}, {"layerNumber", mark.layerNumber}});
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleAddMarks(const transport::ApiRequest& request) const {
        const auto trackName = parsing::ReadString(request.params, "track");
        if (trackName.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.addMarks requires track.", nlohmann::json::object()};
            return response;
        }
        const auto subType = parsing::ReadString(request.params, "subType");
        const bool replaceExisting = parsing::ReadBool(request.params, "replaceExisting", false);
        const auto marksJsonText = parsing::ReadString(request.params, "marks");
        if (marksJsonText.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.addMarks requires marks JSON.", nlohmann::json::object()};
            return response;
        }
        nlohmann::json marksJson;
        try { marksJson = nlohmann::json::parse(marksJsonText); } catch (const std::exception& ex) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.addMarks received invalid marks JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!marksJson.is_array()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.addMarks requires marks to be a JSON array.", nlohmann::json::object()};
            return response;
        }
        models::AddTimingMarksRequest addRequest;
        addRequest.trackName = trackName;
        addRequest.subType = subType;
        addRequest.replaceExisting = replaceExisting;
        for (const auto& item : marksJson) {
            if (!item.is_object() || !item.contains("startMs") || !item.contains("endMs")) {
                transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each timing mark must include startMs and endMs.", {{"item", item}}};
                return response;
            }
            const int startMs = item.value("startMs", -1);
            const int endMs = item.value("endMs", -1);
            if (startMs < 0 || endMs < 0 || endMs < startMs) {
                transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each timing mark must have a valid non-negative range.", {{"item", item}}};
                return response;
            }
            addRequest.marks.push_back({startMs, endMs, item.value("label", "")});
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, addRequest, trackName]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.addMarks(addRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.trackFound) {
                response.statusCode = 500; response.error = transport::ApiError{std::string(transport::errors::InternalError), "Track could not be resolved after timing.addMarks.", {{"track", trackName}}}; return response;
            }
            response.data["requestedTrackName"] = result.requestedTrackName;
            response.data["actualTrackName"] = result.actualTrackName;
            response.data["trackCreated"] = result.trackCreated;
            response.data["addedMarkCount"] = result.addedMarkCount;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleEnsureTrack(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto trackName = parsing::ReadString(request.params, "track");
        const auto subType = parsing::ReadString(request.params, "subType");
        if (trackName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "timing.ensureTrack requires track.", nlohmann::json::object()};
            return response;
        }
        const auto result = _service.ensureTrack({trackName, subType});
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        response.data["requestedTrackName"] = result.requestedTrackName;
        response.data["actualTrackName"] = result.actualTrackName;
        response.data["subType"] = result.subType;
        response.data["created"] = result.created;
        return response;
    }

private:
    services::TimingService _service;
};

} // namespace xLightsDesigner::api::handlers
