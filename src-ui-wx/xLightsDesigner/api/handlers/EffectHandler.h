#pragma once

#include <nlohmann/json.hpp>

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
            response.data["effects"].push_back({{"layerNumber", effect.layerNumber}, {"effectName", effect.effectName}, {"startMs", effect.startMs}, {"endMs", effect.endMs}});
        }
        return response;
    }

private:
    services::EffectService _service;
};

} // namespace xLightsDesigner::api::handlers
