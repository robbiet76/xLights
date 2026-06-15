#pragma once

#include <utility>

#include <nlohmann/json.hpp>

#include "../parsing/ParameterReaders.h"
#include "../services/TimingService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

// Timing endpoints read visible xLights timing tracks. Track names are labels;
// semantic meaning belongs in the app handoff metadata.
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
            response.data["tracks"].push_back({
                {"name", track.name},
                {"type", track.type},
                {"subType", track.subType},
                {"markCount", track.markCount},
                {"layerCount", track.layerCount},
                {"revisionToken", track.revisionToken}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetMarks(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto trackName = readTrackName(request);
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (trackName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "timing.getMarks requires trackName.",
                nlohmann::json{{"acceptedAliases", {"trackName", "track"}}}
            };
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
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "Requested timing track was not found in the current sequence.",
                {{"trackName", trackName}, {"track", trackName}}
            };
            return response;
        }
        response.data["track"] = summary.trackName;
        response.data["trackName"] = summary.trackName;
        response.data["revisionToken"] = summary.revisionToken;
        response.data["marks"] = nlohmann::json::array();
        for (const auto& mark : summary.marks) {
            response.data["marks"].push_back({{"startMs", mark.startMs}, {"endMs", mark.endMs}, {"label", mark.label}, {"layerNumber", mark.layerNumber}});
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetSongStructure(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto summary = _service.getSongStructure();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }

        response.data["hasRegions"] = summary.hasRegions;
        response.data["activeViewIndex"] = summary.activeViewIndex;
        response.data["activeViewName"] = summary.activeViewName;
        response.data["revisionToken"] = summary.revisionToken;
        response.data["views"] = nlohmann::json::array();
        for (const auto& view : summary.views) {
            nlohmann::json regions = nlohmann::json::array();
            for (const auto& region : view.regions) {
                regions.push_back({
                    {"id", region.id},
                    {"startMs", region.startMs},
                    {"endMs", region.endMs},
                    {"name", region.name},
                    {"colorARGB", region.colorARGB}
                });
            }
            response.data["views"].push_back({
                {"viewIndex", view.viewIndex},
                {"name", view.name},
                {"active", view.active},
                {"regions", regions}
            });
        }
        return response;
    }

private:
    [[nodiscard]] static std::string readTrackName(const transport::ApiRequest& request) {
        auto trackName = parsing::ReadString(request.params, "trackName");
        if (trackName.empty()) {
            trackName = parsing::ReadString(request.params, "track");
        }
        return trackName;
    }

    services::TimingService _service;
};

} // namespace xLightsDesigner::api::handlers
