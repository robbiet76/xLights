#pragma once

#include <nlohmann/json.hpp>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/SequencingService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

namespace sequencing_handler_detail {
inline std::string ReadOptionalJsonText(const nlohmann::json& item, const char* key) {
    if (!item.is_object() || !item.contains(key) || item.at(key).is_null()) {
        return std::string();
    }
    const auto& value = item.at(key);
    if (value.is_string()) {
        return value.get<std::string>();
    }
    return value.dump();
}
}

class SequencingHandler {
public:
    explicit SequencingHandler(services::SequencingService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleApplyWindowPlan(const transport::ApiRequest& request) const {
        const auto track = parsing::ReadString(request.params, "track");
        const auto subType = parsing::ReadString(request.params, "subType");
        const auto element = parsing::ReadString(request.params, "element");
        const auto effectName = parsing::ReadString(request.params, "effectName");
        const auto settings = parsing::ReadString(request.params, "settings");
        const auto palette = parsing::ReadString(request.params, "palette");
        const int layer = parsing::ReadInt(request.params, "layer", -1);
        const int effectStartMs = parsing::ReadInt(request.params, "effectStartMs", -1);
        const int effectEndMs = parsing::ReadInt(request.params, "effectEndMs", -1);
        const bool replaceExistingMarks = parsing::ReadBool(request.params, "replaceExistingMarks", false);
        const auto marksJsonText = parsing::ReadString(request.params, "marks");
        if (track.empty() || element.empty() || effectName.empty() || layer < 0 || effectStartMs < 0 || effectEndMs < 0 || effectEndMs < effectStartMs || marksJsonText.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyWindowPlan requires track, marks, element, effectName, layer, effectStartMs, and effectEndMs.", nlohmann::json::object()};
            return response;
        }
        nlohmann::json marksJson;
        try { marksJson = nlohmann::json::parse(marksJsonText); } catch (const std::exception& ex) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyWindowPlan received invalid marks JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!marksJson.is_array()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyWindowPlan requires marks to be a JSON array.", nlohmann::json::object()};
            return response;
        }
        if (marksJson.size() < 2) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{"FULL_TRACK_CONTEXT_REQUIRED", "sequencing.applyWindowPlan requires the full section set, not a single timing mark.", nlohmann::json{{"markCount", marksJson.size()}}};
            return response;
        }
        models::SequencingApplyWindowPlanRequest plan;
        plan.ensureTrack.trackName = track;
        plan.ensureTrack.subType = subType;
        plan.addMarks.trackName = track;
        plan.addMarks.subType = subType;
        plan.addMarks.replaceExisting = replaceExistingMarks;
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
            plan.addMarks.marks.push_back({startMs, endMs, item.value("label", "")});
        }
        plan.clearWindow.elementName = element;
        plan.clearWindow.layerNumber = layer;
        plan.clearWindow.startMs = effectStartMs;
        plan.clearWindow.endMs = effectEndMs;
        plan.addEffect.elementName = element;
        plan.addEffect.layerNumber = layer;
        plan.addEffect.effectName = effectName;
        plan.addEffect.settings = settings;
        plan.addEffect.palette = palette;
        plan.addEffect.startMs = effectStartMs;
        plan.addEffect.endMs = effectEndMs;

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, plan, element, layer]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.applyWindowPlan(plan);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.clearWindow.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", element}}}; return response;
            }
            if (!result.clearWindow.layerFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested layer was not found on the current element.", {{"element", element}, {"layer", layer}}}; return response;
            }
            if (!result.ok) {
                if (result.errorCode.has_value() && result.errorMessage.has_value()) {
                    response.statusCode = 400;
                    response.error = transport::ApiError{*result.errorCode, *result.errorMessage, nlohmann::json{{"track", result.ensureTrack.requestedTrackName}, {"markCount", plan.addMarks.marks.size()}}};
                    if (!result.warnings.empty()) response.warnings = result.warnings;
                    return response;
                }
                response.statusCode = 500; response.error = transport::ApiError{std::string(transport::errors::InternalError), "sequencing.applyWindowPlan did not complete successfully.", nlohmann::json::object()}; return response;
            }
            response.data["ok"] = result.ok;
            response.data["track"] = result.ensureTrack.actualTrackName;
            response.data["trackCreated"] = result.ensureTrack.created;
            response.data["addedMarkCount"] = result.addMarks.addedMarkCount;
            response.data["clearedEffectCount"] = result.clearWindow.clearedEffectCount;
            response.data["effect"] = {{"element", result.addEffect.elementName}, {"layer", result.addEffect.layerNumber}, {"effectName", result.addEffect.effectName}, {"startMs", result.addEffect.startMs}, {"endMs", result.addEffect.endMs}, {"created", result.addEffect.created}};
            if (!result.warnings.empty()) response.warnings = result.warnings;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleApplyBatchPlan(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto track = parsing::ReadString(request.params, "track");
        const auto subType = parsing::ReadString(request.params, "subType");
        const bool replaceExistingMarks = parsing::ReadBool(request.params, "replaceExistingMarks", false);
        const auto marksJsonText = parsing::ReadString(request.params, "marks");
        const auto effectsJsonText = parsing::ReadString(request.params, "effects");
        if (track.empty() || marksJsonText.empty() || effectsJsonText.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyBatchPlan requires track, marks, and effects JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json marksJson;
        try {
            marksJson = nlohmann::json::parse(marksJsonText);
        } catch (const std::exception& ex) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyBatchPlan received invalid marks JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!marksJson.is_array()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyBatchPlan requires marks to be a JSON array.", nlohmann::json::object()};
            return response;
        }
        if (marksJson.size() < 2) {
            response.statusCode = 400;
            response.error = transport::ApiError{"FULL_TRACK_CONTEXT_REQUIRED", "sequencing.applyBatchPlan requires the full section set, not a single timing mark.", nlohmann::json{{"markCount", marksJson.size()}}};
            return response;
        }

        nlohmann::json effectsJson;
        try {
            effectsJson = nlohmann::json::parse(effectsJsonText);
        } catch (const std::exception& ex) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyBatchPlan received invalid effects JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!effectsJson.is_array() || effectsJson.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "sequencing.applyBatchPlan requires effects to be a non-empty JSON array.", nlohmann::json::object()};
            return response;
        }

        models::SequencingApplyBatchPlanRequest plan;
        plan.ensureTrack.trackName = track;
        plan.ensureTrack.subType = subType;
        plan.addMarks.trackName = track;
        plan.addMarks.subType = subType;
        plan.addMarks.replaceExisting = replaceExistingMarks;

        for (const auto& item : marksJson) {
            if (!item.is_object() || !item.contains("startMs") || !item.contains("endMs")) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each timing mark must include startMs and endMs.", {{"item", item}}};
                return response;
            }
            const int startMs = item.value("startMs", -1);
            const int endMs = item.value("endMs", -1);
            if (startMs < 0 || endMs < 0 || endMs < startMs) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each timing mark must have a valid non-negative range.", {{"item", item}}};
                return response;
            }
            plan.addMarks.marks.push_back({startMs, endMs, item.value("label", "")});
        }

        for (std::size_t index = 0; index < effectsJson.size(); ++index) {
            const auto& item = effectsJson.at(index);
            if (!item.is_object()) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each batch effect must be an object.", {{"index", static_cast<int>(index)}}};
                return response;
            }
            const auto elementName = item.value("element", std::string());
            const auto effectName = item.value("effectName", std::string());
            const auto settings = sequencing_handler_detail::ReadOptionalJsonText(item, "settings");
            const auto palette = sequencing_handler_detail::ReadOptionalJsonText(item, "palette");
            const int layerNumber = item.value("layer", -1);
            const int startMs = item.value("startMs", -1);
            const int endMs = item.value("endMs", -1);
            const bool clearExisting = item.value("clearExisting", false);
            if (elementName.empty() || effectName.empty() || layerNumber < 0 || startMs < 0 || endMs < 0 || endMs < startMs) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each batch effect requires element, effectName, layer, startMs, and endMs with a valid non-negative range.", {{"index", static_cast<int>(index)}, {"element", elementName}, {"effectName", effectName}, {"layer", layerNumber}, {"startMs", startMs}, {"endMs", endMs}}};
                return response;
            }
            plan.effectBatch.effects.push_back({elementName, layerNumber, effectName, settings, palette, startMs, endMs, clearExisting});
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, plan, track]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.applyBatchPlan(plan);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404;
                queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return queuedResponse;
            }
            if (!result.ok) {
                const auto code = result.errorCode.value_or(std::string(transport::errors::ValidationError));
                const auto message = result.errorMessage.value_or(std::string("sequencing.applyBatchPlan failed."));
                nlohmann::json details = {{"track", result.ensureTrack.requestedTrackName.empty() ? track : result.ensureTrack.requestedTrackName}, {"markCount", plan.addMarks.marks.size()}, {"requestedCount", result.effectBatch.requestedCount}, {"createdCount", result.effectBatch.createdCount}, {"clearedEffectCount", result.effectBatch.clearedEffectCount}};
                if (result.effectBatch.failedItemIndex.has_value()) {
                    details["failedItemIndex"] = *result.effectBatch.failedItemIndex;
                }
                queuedResponse.statusCode = 400;
                queuedResponse.error = transport::ApiError{code, message, details};
                if (!result.warnings.empty()) queuedResponse.warnings = result.warnings;
                return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["track"] = result.ensureTrack.actualTrackName;
            queuedResponse.data["trackCreated"] = result.ensureTrack.created;
            queuedResponse.data["addedMarkCount"] = result.addMarks.addedMarkCount;
            queuedResponse.data["requestedCount"] = result.effectBatch.requestedCount;
            queuedResponse.data["createdCount"] = result.effectBatch.createdCount;
            queuedResponse.data["clearedEffectCount"] = result.effectBatch.clearedEffectCount;
            queuedResponse.data["effects"] = nlohmann::json::array();
            for (const auto& item : result.effectBatch.items) {
                queuedResponse.data["effects"].push_back({{"element", item.elementName}, {"layer", item.layerNumber}, {"effectName", item.effectName}, {"startMs", item.startMs}, {"endMs", item.endMs}, {"clearExisting", item.clearExisting}, {"clearedEffectCount", item.clearedEffectCount}, {"created", item.created}});
            }
            if (!result.warnings.empty()) queuedResponse.warnings = result.warnings;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

private:
    services::SequencingService _service;
};

} // namespace xLightsDesigner::api::handlers
