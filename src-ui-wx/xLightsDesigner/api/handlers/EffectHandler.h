#pragma once

#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/EffectService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

// Native effect endpoints expose xLights' own effect/layer primitives. The API
// does not interpret effect-specific settings; callers provide the same effect
// name, settings, palette, layer, and time values that xLights stores.
class EffectHandler {
public:
    explicit EffectHandler(services::EffectService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleListSchemas(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::NativeEffectSchemaRequest schemaRequest;
        const auto effectNamesIt = request.params.find("effectNames");
        if (effectNamesIt != request.params.end()) {
            const std::string& names = effectNamesIt->second;
            if (!names.empty() && names.front() == '[') {
                const auto parsedNames = nlohmann::json::parse(names, nullptr, false);
                if (parsedNames.is_array()) {
                    for (const auto& value : parsedNames) {
                        if (value.is_string()) {
                            schemaRequest.effectNames.push_back(value.get<std::string>());
                        }
                    }
                }
            } else {
                std::stringstream stream(names);
                std::string effectName;
                while (std::getline(stream, effectName, ',')) {
                    if (!effectName.empty()) {
                        schemaRequest.effectNames.push_back(effectName);
                    }
                }
            }
        }
        const auto result = _service.listSchemas(schemaRequest);
        response.data["revisionToken"] = result.revisionToken;
        response.data["schemas"] = nlohmann::json::array();
        for (const auto& schema : result.schemas) {
            response.data["schemas"].push_back(schemaToJson(schema));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleListEffects(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::NativeEffectListRequest listRequest;
        listRequest.elementName = parsing::ReadString(request.params, "elementName");
        listRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        const int startMs = parsing::ReadInt(request.params, "startMs", -1);
        const int endMs = parsing::ReadInt(request.params, "endMs", -1);
        if (startMs >= 0) listRequest.startMs = startMs;
        if (endMs >= 0) listRequest.endMs = endMs;
        listRequest.xldOnly = parsing::ReadBool(request.params, "xldOnly", false);
        listRequest.includeSettings = parsing::ReadBool(request.params, "includeSettings", true);

        const auto result = _service.listEffects(listRequest);
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        if ((!listRequest.elementName.empty() || !listRequest.submodelName.empty()) && !result.elementFound) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested sequence element or submodel was not found.", {{"elementName", listRequest.elementName}, {"submodelName", listRequest.submodelName}}};
            return response;
        }
        response.data["effects"] = nlohmann::json::array();
        for (const auto& effect : result.effects) {
            response.data["effects"].push_back(effectToJson(effect, listRequest.includeSettings));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleUpsertEffect(const transport::ApiRequest& request) const {
        models::NativeEffectUpsertRequest effectRequest;
        effectRequest.elementName = parsing::ReadString(request.params, "elementName");
        effectRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        effectRequest.layerIndex = parsing::ReadInt(request.params, "layerIndex", 0);
        effectRequest.nativeId = parsing::ReadInt(request.params, "nativeId", -1);
        effectRequest.xldId = parsing::ReadString(request.params, "xldId");
        effectRequest.xldOwner = parsing::ReadString(request.params, "xldOwner");
        if (effectRequest.xldOwner.empty()) effectRequest.xldOwner = "xLightsDesigner";
        effectRequest.effectName = parsing::ReadString(request.params, "effectName");
        effectRequest.startMs = parsing::ReadInt(request.params, "startMs", -1);
        effectRequest.endMs = parsing::ReadInt(request.params, "endMs", -1);
        effectRequest.settings = parsing::ReadString(request.params, "settings");
        effectRequest.palette = parsing::ReadString(request.params, "palette");
        effectRequest.replaceExistingXld = parsing::ReadBool(request.params, "replaceExistingXld", true);

        if (effectRequest.elementName.empty() || effectRequest.effectName.empty() || effectRequest.layerIndex < 0 || effectRequest.startMs < 0 || effectRequest.endMs < effectRequest.startMs) {
            return validationError(request, "effects.upsert requires elementName, effectName, layerIndex >= 0, and a valid startMs/endMs range.");
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, effectRequest]() {
            return mutationResponse(request, service.upsertEffect(effectRequest));
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleRemoveEffect(const transport::ApiRequest& request) const {
        models::NativeEffectRemoveRequest removeRequest;
        removeRequest.elementName = parsing::ReadString(request.params, "elementName");
        removeRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        removeRequest.layerIndex = parsing::ReadInt(request.params, "layerIndex", 0);
        removeRequest.nativeId = parsing::ReadInt(request.params, "nativeId", -1);
        removeRequest.xldId = parsing::ReadString(request.params, "xldId");
        removeRequest.allowUserOwned = parsing::ReadBool(request.params, "allowUserOwned", false);
        if (removeRequest.elementName.empty() || removeRequest.layerIndex < 0 || (removeRequest.nativeId < 0 && removeRequest.xldId.empty())) {
            return validationError(request, "effects.remove requires elementName, layerIndex >= 0, plus nativeId or xldId.");
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, removeRequest]() {
            return mutationResponse(request, service.removeEffect(removeRequest));
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleListLayers(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        models::NativeEffectLayerListRequest listRequest;
        listRequest.elementName = parsing::ReadString(request.params, "elementName");
        listRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        const auto result = _service.listLayers(listRequest);
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        if ((!listRequest.elementName.empty() || !listRequest.submodelName.empty()) && !result.elementFound) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "Requested sequence element or submodel was not found.", {{"elementName", listRequest.elementName}, {"submodelName", listRequest.submodelName}}};
            return response;
        }
        response.data["layers"] = nlohmann::json::array();
        for (const auto& layer : result.layers) {
            response.data["layers"].push_back(layerToJson(layer));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleEnsureLayer(const transport::ApiRequest& request) const {
        models::NativeEffectLayerEnsureRequest layerRequest;
        layerRequest.elementName = parsing::ReadString(request.params, "elementName");
        layerRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        layerRequest.layerIndex = parsing::ReadInt(request.params, "layerIndex", -1);
        layerRequest.layerName = parsing::ReadString(request.params, "layerName");
        if (layerRequest.elementName.empty()) {
            return validationError(request, "effects.ensureLayer requires elementName.");
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, layerRequest]() {
            return layerMutationResponse(request, service.ensureLayer(layerRequest));
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleRemoveLayer(const transport::ApiRequest& request) const {
        models::NativeEffectLayerRemoveRequest layerRequest;
        layerRequest.elementName = parsing::ReadString(request.params, "elementName");
        layerRequest.submodelName = parsing::ReadString(request.params, "submodelName");
        layerRequest.layerIndex = parsing::ReadInt(request.params, "layerIndex", -1);
        layerRequest.allowUserOwned = parsing::ReadBool(request.params, "allowUserOwned", false);
        if (layerRequest.elementName.empty() || layerRequest.layerIndex < 0) {
            return validationError(request, "effects.removeLayer requires elementName and layerIndex.");
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, layerRequest]() {
            return layerMutationResponse(request, service.removeLayer(layerRequest));
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

private:
    [[nodiscard]] static nlohmann::json effectToJson(const models::NativeEffectSummary& effect, bool includeSettings) {
        nlohmann::json result = {
            {"id", effect.id},
            {"elementName", effect.elementName},
            {"submodelName", effect.submodelName},
            {"layerIndex", effect.layerIndex},
            {"effectIndex", effect.effectIndex},
            {"nativeId", effect.nativeId},
            {"effectName", effect.effectName},
            {"startMs", effect.startMs},
            {"endMs", effect.endMs},
            {"protected", effect.protectedEffect},
            {"locked", effect.locked},
            {"renderDisabled", effect.renderDisabled},
            {"xldOwned", effect.xldOwned},
            {"xldId", effect.xldId},
            {"xldOwner", effect.xldOwner}
        };
        if (includeSettings) {
            result["settings"] = effect.settings;
            result["palette"] = effect.palette;
        }
        return result;
    }

    [[nodiscard]] static nlohmann::json schemaToJson(const models::NativeEffectSchema& schema) {
        nlohmann::json result = {
            {"effectName", schema.effectName},
            {"canvasMode", schema.canvasMode},
            {"properties", schema.properties.is_array() ? schema.properties : nlohmann::json::array()},
            {"groups", schema.groups.is_array() ? schema.groups : nlohmann::json::array()},
            {"visibilityRules", schema.visibilityRules.is_array() ? schema.visibilityRules : nlohmann::json::array()}
        };
        if (schema.rawMetadata.is_object() && !schema.rawMetadata.empty()) {
            result["metadata"] = schema.rawMetadata;
        }
        return result;
    }

    [[nodiscard]] static nlohmann::json layerToJson(const models::NativeEffectLayerSummary& layer) {
        return {
            {"elementName", layer.elementName},
            {"submodelName", layer.submodelName},
            {"layerIndex", layer.layerIndex},
            {"layerNumber", layer.layerNumber},
            {"layerName", layer.layerName},
            {"effectCount", layer.effectCount},
            {"hasUserOwnedEffects", layer.hasUserOwnedEffects},
            {"hasXldOwnedEffects", layer.hasXldOwnedEffects}
        };
    }

    [[nodiscard]] static transport::ApiResponse validationError(const transport::ApiRequest& request, const std::string& message) {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        response.statusCode = 400;
        response.error = transport::ApiError{std::string(transport::errors::ValidationError), message, nlohmann::json::object()};
        return response;
    }

    [[nodiscard]] static transport::ApiResponse mutationResponse(const transport::ApiRequest& request, const models::NativeEffectMutationResult& result) {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        if (!result.ok) {
            response.statusCode = result.elementFound ? 400 : 404;
            response.error = transport::ApiError{
                result.errorCode.empty() ? std::string(transport::errors::ValidationError) : result.errorCode,
                result.errorMessage.empty() ? "Native effect mutation failed." : result.errorMessage,
                nlohmann::json::object()
            };
            return response;
        }
        response.data["ok"] = true;
        response.data["created"] = result.created;
        response.data["updated"] = result.updated;
        response.data["removed"] = result.removed;
        response.data["effect"] = effectToJson(result.effect, true);
        return response;
    }

    [[nodiscard]] static transport::ApiResponse layerMutationResponse(const transport::ApiRequest& request, const models::NativeEffectLayerMutationResult& result) {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }
        if (!result.ok) {
            response.statusCode = result.elementFound ? 400 : 404;
            response.error = transport::ApiError{
                result.errorCode.empty() ? std::string(transport::errors::ValidationError) : result.errorCode,
                result.errorMessage.empty() ? "Native effect layer mutation failed." : result.errorMessage,
                nlohmann::json::object()
            };
            return response;
        }
        response.data["ok"] = true;
        response.data["created"] = result.created;
        response.data["removed"] = result.removed;
        response.data["layer"] = layerToJson(result.layer);
        return response;
    }

    services::EffectService _service;
};

} // namespace xLightsDesigner::api::handlers
