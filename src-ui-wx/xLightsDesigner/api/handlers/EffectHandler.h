#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <optional>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/EffectService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

namespace effect_handler_detail {
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

inline std::optional<int> ReadOptionalInt(const std::map<std::string, std::string>& params, const char* key) {
    const auto it = params.find(key);
    if (it == params.end() || it->second.empty()) return std::nullopt;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return std::nullopt;
    }
}
}

class EffectHandler {
public:
    explicit EffectHandler(services::EffectService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleClearWindow(const transport::ApiRequest& request) const {
        const auto elementName = parsing::ReadString(request.params, "element");
        const int layerNumber = parsing::ReadInt(request.params, "layer", -1);
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (elementName.empty() || layerNumber < 0 || startMs < 0 || endMs < 0 || endMs < startMs) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.clearWindow requires element, layer, startMs, and endMs with a valid non-negative range.", {{"element", elementName}, {"layer", layerNumber}, {"startMs", startMs}, {"endMs", endMs}}};
            return response;
        }
        const auto clearRequest = models::ClearEffectWindowRequest{elementName, layerNumber, startMs, endMs};
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, clearRequest, elementName, layerNumber]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.clearWindow(clearRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", elementName}}}; return response;
            }
            if (!result.layerFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested layer was not found on the current element.", {{"element", elementName}, {"layer", layerNumber}}}; return response;
            }
            response.data["element"] = result.elementName;
            response.data["layer"] = result.layerNumber;
            response.data["startMs"] = result.startMs;
            response.data["endMs"] = result.endMs;
            response.data["clearedEffectCount"] = result.clearedEffectCount;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleAddEffect(const transport::ApiRequest& request) const {
        const auto elementName = parsing::ReadString(request.params, "element");
        const auto effectName = parsing::ReadString(request.params, "effectName");
        const auto settings = parsing::ReadString(request.params, "settings");
        const auto palette = parsing::ReadString(request.params, "palette");
        const int layerNumber = parsing::ReadInt(request.params, "layer", -1);
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (elementName.empty() || effectName.empty() || layerNumber < 0 || startMs < 0 || endMs < 0 || endMs < startMs) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.addEffect requires element, effectName, layer, startMs, and endMs with a valid non-negative range.", {{"element", elementName}, {"effectName", effectName}, {"layer", layerNumber}, {"startMs", startMs}, {"endMs", endMs}}};
            return response;
        }
        const auto addRequest = models::AddEffectRequest{elementName, layerNumber, effectName, settings, palette, startMs, endMs};
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, addRequest, elementName, effectName]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.addEffect(addRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", elementName}}}; return response;
            }
            if (!result.created) {
                response.statusCode = 500; response.error = transport::ApiError{std::string(transport::errors::InternalError), "Effect could not be created.", {{"element", elementName}, {"effectName", effectName}}}; return response;
            }
            response.data["element"] = result.elementName;
            response.data["layer"] = result.layerNumber;
            response.data["effectName"] = result.effectName;
            response.data["startMs"] = result.startMs;
            response.data["endMs"] = result.endMs;
            response.data["created"] = result.created;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleApplyBatch(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto effectsJsonText = parsing::ReadString(request.params, "effects");
        if (effectsJsonText.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.applyBatch requires effects JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json effectsJson;
        try {
            effectsJson = nlohmann::json::parse(effectsJsonText);
        } catch (const std::exception& ex) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.applyBatch received invalid effects JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!effectsJson.is_array() || effectsJson.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.applyBatch requires effects to be a non-empty JSON array.", nlohmann::json::object()};
            return response;
        }

        models::ApplyEffectBatchRequest batchRequest;
        for (std::size_t index = 0; index < effectsJson.size(); ++index) {
            const auto& item = effectsJson.at(index);
            if (!item.is_object()) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each batch effect must be an object.", {{"index", static_cast<int>(index)}}};
                return response;
            }
            const auto elementName = item.value("element", std::string());
            const auto effectName = item.value("effectName", std::string());
            const auto settings = effect_handler_detail::ReadOptionalJsonText(item, "settings");
            const auto palette = effect_handler_detail::ReadOptionalJsonText(item, "palette");
            const int layerNumber = item.value("layer", -1);
            const int startMs = item.value("startMs", -1);
            const int endMs = item.value("endMs", -1);
            const bool clearExisting = item.value("clearExisting", false);
            if (elementName.empty() || effectName.empty() || layerNumber < 0 || startMs < 0 || endMs < 0 || endMs < startMs) {
                response.statusCode = 400;
                response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Each batch effect requires element, effectName, layer, startMs, and endMs with a valid non-negative range.", {{"index", static_cast<int>(index)}, {"element", elementName}, {"effectName", effectName}, {"layer", layerNumber}, {"startMs", startMs}, {"endMs", endMs}}};
                return response;
            }
            batchRequest.effects.push_back({elementName, layerNumber, effectName, settings, palette, startMs, endMs, clearExisting});
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, batchRequest]() {
            transport::ApiResponse response;
            response.command = request.command;
            response.requestId = request.requestId;
            const auto result = service.applyBatch(batchRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404;
                response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return response;
            }
            if (!result.ok) {
                response.statusCode = 400;
                nlohmann::json details = {{"requestedCount", result.requestedCount}, {"createdCount", result.createdCount}, {"clearedEffectCount", result.clearedEffectCount}};
                if (result.failedItemIndex.has_value()) {
                    details["failedItemIndex"] = *result.failedItemIndex;
                }
                response.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.applyBatch failed."), details};
                return response;
            }
            response.data["ok"] = true;
            response.data["requestedCount"] = result.requestedCount;
            response.data["createdCount"] = result.createdCount;
            response.data["clearedEffectCount"] = result.clearedEffectCount;
            response.data["effects"] = nlohmann::json::array();
            for (const auto& item : result.items) {
                response.data["effects"].push_back({{"element", item.elementName}, {"layer", item.layerNumber}, {"effectName", item.effectName}, {"startMs", item.startMs}, {"endMs", item.endMs}, {"clearExisting", item.clearExisting}, {"clearedEffectCount", item.clearedEffectCount}, {"created", item.created}});
            }
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleGetWindow(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto elementName = parsing::ReadString(request.params, "element");
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (elementName.empty() || startMs < 0 || endMs < 0 || endMs < startMs) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.getWindow requires element, startMs, and endMs with a valid non-negative range.", {{"element", elementName}, {"startMs", startMs}, {"endMs", endMs}}};
            return response;
        }
        const auto summary = _service.getWindow({elementName, startMs, endMs});
        if (!summary.sequenceOpen) {
            response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
        }
        if (!summary.elementFound) {
            response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", elementName}}}; return response;
        }
        response.data["element"] = summary.elementName;
        response.data["startMs"] = summary.startMs;
        response.data["endMs"] = summary.endMs;
        response.data["effects"] = nlohmann::json::array();
        for (const auto& effect : summary.effects) {
            response.data["effects"].push_back({{"effectId", effect.effectId}, {"layerNumber", effect.layerNumber}, {"effectName", effect.effectName}, {"startMs", effect.startMs}, {"endMs", effect.endMs}, {"settings", effect.settings}, {"palette", effect.palette}});
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleUpdateEffect(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto elementName = parsing::ReadString(request.params, "element");
        const auto effectName = parsing::ReadString(request.params, "effectName");
        if (elementName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.update requires element plus effectId or layer/startMs/endMs selector fields.", nlohmann::json::object()};
            return response;
        }
        models::UpdateEffectRequest updateRequest;
        updateRequest.selector.elementName = elementName;
        updateRequest.selector.effectId = effect_handler_detail::ReadOptionalInt(request.params, "effectId");
        updateRequest.selector.layerNumber = effect_handler_detail::ReadOptionalInt(request.params, "layer");
        updateRequest.selector.startMs = effect_handler_detail::ReadOptionalInt(request.params, "startMs");
        updateRequest.selector.endMs = effect_handler_detail::ReadOptionalInt(request.params, "endMs");
        updateRequest.selector.effectName = effectName;
        updateRequest.layerNumber = effect_handler_detail::ReadOptionalInt(request.params, "newLayer");
        updateRequest.startMs = effect_handler_detail::ReadOptionalInt(request.params, "newStartMs");
        updateRequest.endMs = effect_handler_detail::ReadOptionalInt(request.params, "newEndMs");
        const auto newEffectName = parsing::ReadString(request.params, "newEffectName");
        const auto settings = parsing::ReadString(request.params, "settings");
        const auto palette = parsing::ReadString(request.params, "palette");
        if (!newEffectName.empty()) updateRequest.effectName = newEffectName;
        if (!settings.empty()) updateRequest.settings = settings;
        if (!palette.empty()) updateRequest.palette = palette;
        if (!updateRequest.selector.effectId.has_value() && (!updateRequest.selector.layerNumber.has_value() || !updateRequest.selector.startMs.has_value() || !updateRequest.selector.endMs.has_value())) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.update requires effectId or layer/startMs/endMs selector fields.", nlohmann::json::object()};
            return response;
        }
        if (!updateRequest.layerNumber.has_value() && !updateRequest.startMs.has_value() && !updateRequest.endMs.has_value() && !updateRequest.effectName.has_value() && !updateRequest.settings.has_value() && !updateRequest.palette.has_value()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.update requires at least one new value.", nlohmann::json::object()};
            return response;
        }
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, updateRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.updateEffect(updateRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404; queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return queuedResponse;
            }
            if (!result.elementFound) {
                queuedResponse.statusCode = 404; queuedResponse.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", updateRequest.selector.elementName}}}; return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400; queuedResponse.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.update failed."), {{"matchedCount", result.matchedCount}, {"updatedCount", result.updatedCount}}}; return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["matchedCount"] = result.matchedCount;
            queuedResponse.data["updatedCount"] = result.updatedCount;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleDeleteEffects(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto elementName = parsing::ReadString(request.params, "element");
        if (elementName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.delete requires element plus effectId or layer/startMs/endMs selector fields.", nlohmann::json::object()};
            return response;
        }
        models::DeleteEffectsRequest deleteRequest;
        deleteRequest.selector.elementName = elementName;
        deleteRequest.selector.effectId = effect_handler_detail::ReadOptionalInt(request.params, "effectId");
        deleteRequest.selector.layerNumber = effect_handler_detail::ReadOptionalInt(request.params, "layer");
        deleteRequest.selector.startMs = effect_handler_detail::ReadOptionalInt(request.params, "startMs");
        deleteRequest.selector.endMs = effect_handler_detail::ReadOptionalInt(request.params, "endMs");
        deleteRequest.selector.effectName = parsing::ReadString(request.params, "effectName");
        if (!deleteRequest.selector.effectId.has_value() && (!deleteRequest.selector.layerNumber.has_value() || !deleteRequest.selector.startMs.has_value() || !deleteRequest.selector.endMs.has_value())) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.delete requires effectId or layer/startMs/endMs selector fields.", nlohmann::json::object()};
            return response;
        }
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, deleteRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.deleteEffects(deleteRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404; queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return queuedResponse;
            }
            if (!result.elementFound) {
                queuedResponse.statusCode = 404; queuedResponse.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", deleteRequest.selector.elementName}}}; return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400; queuedResponse.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.delete failed."), {{"deletedCount", result.deletedCount}}}; return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["deletedCount"] = result.deletedCount;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleDeleteLayer(const transport::ApiRequest& request) const {
        const auto elementName = parsing::ReadString(request.params, "element");
        const int layerNumber = parsing::ReadInt(request.params, "layer", -1);
        const bool force = parsing::ReadBool(request.params, "force", false);
        if (elementName.empty() || layerNumber < 0) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.deleteLayer requires element and layer.", {{"element", elementName}, {"layer", layerNumber}}};
            return response;
        }
        const auto deleteRequest = models::DeleteEffectLayerRequest{elementName, layerNumber, force};
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, deleteRequest]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.deleteLayer(deleteRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", deleteRequest.elementName}}}; return response;
            }
            if (!result.layerFound || !result.ok) {
                response.statusCode = 400; response.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.deleteLayer failed."), {{"layer", result.layerNumber}, {"layerCount", result.layerCount}, {"effectCount", result.effectCount}}}; return response;
            }
            response.data["ok"] = true;
            response.data["layer"] = result.layerNumber;
            response.data["layerCount"] = result.layerCount;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleReorderLayer(const transport::ApiRequest& request) const {
        const auto elementName = parsing::ReadString(request.params, "element");
        const int fromLayer = parsing::ReadInt(request.params, "fromLayer", -1);
        const int toLayer = parsing::ReadInt(request.params, "toLayer", -1);
        if (elementName.empty() || fromLayer < 0 || toLayer < 0) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.reorderLayer requires element, fromLayer, and toLayer.", {{"element", elementName}, {"fromLayer", fromLayer}, {"toLayer", toLayer}}};
            return response;
        }
        const auto reorderRequest = models::ReorderEffectLayerRequest{elementName, fromLayer, toLayer};
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, reorderRequest]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.reorderLayer(reorderRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", reorderRequest.elementName}}}; return response;
            }
            if (!result.ok) {
                response.statusCode = 400; response.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.reorderLayer failed."), {{"fromLayer", result.fromLayer}, {"toLayer", result.toLayer}, {"layerCount", result.layerCount}}}; return response;
            }
            response.data["ok"] = true;
            response.data["fromLayer"] = result.fromLayer;
            response.data["toLayer"] = result.toLayer;
            response.data["layerCount"] = result.layerCount;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleCompactLayers(const transport::ApiRequest& request) const {
        const auto elementName = parsing::ReadString(request.params, "element");
        if (elementName.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "effects.compactLayers requires element.", nlohmann::json::object()};
            return response;
        }
        const auto compactRequest = models::CompactEffectLayersRequest{elementName};
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, compactRequest]() {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId;
            const auto result = service.compactLayers(compactRequest);
            if (!result.sequenceOpen) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()}; return response;
            }
            if (!result.elementFound) {
                response.statusCode = 404; response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested element was not found in the current sequence.", {{"element", compactRequest.elementName}}}; return response;
            }
            if (!result.ok) {
                response.statusCode = 400; response.error = transport::ApiError{result.errorCode.value_or(std::string(transport::errors::ValidationError)), result.errorMessage.value_or("effects.compactLayers failed."), {{"layerCount", result.layerCount}}}; return response;
            }
            response.data["ok"] = true;
            response.data["layerCount"] = result.layerCount;
            response.data["removedLayerNumbers"] = result.removedLayerNumbers;
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

private:
    services::EffectService _service;
};

} // namespace xLightsDesigner::api::handlers
