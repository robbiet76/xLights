#pragma once

#include <map>
#include <utility>

#include "../parsing/ParameterReaders.h"
#include "../services/LayoutService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"

namespace xLightsDesigner::api::handlers {

// Layout endpoints expose the physical display skeleton used by native-effect
// generation: models, submodels, groups, node coordinates, and channel spans.
class LayoutHandler {
public:
    explicit LayoutHandler(services::LayoutService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetModels(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getModels();
        response.data["models"] = nlohmann::json::array();
        for (const auto& model : summary.models) {
            const auto optionalJson = [](const auto& value) -> nlohmann::json {
                return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
            };
            response.data["models"].push_back({
                {"name", model.name},
                {"displayAs", model.displayAs},
                {"stringType", model.stringType},
                {"layoutGroup", model.layoutGroup},
                {"startChannel", model.startChannel},
                {"endChannel", model.endChannel},
                {"submodelCount", model.submodelCount},
                {"nodeCount", model.nodeCount},
                {"previewPixelSize", model.previewPixelSize},
                {"positionX", model.positionX},
                {"positionY", model.positionY},
                {"positionZ", model.positionZ},
                {"width", model.width},
                {"height", model.height},
                {"depth", model.depth},
                {"rotationX", model.rotationX},
                {"rotationY", model.rotationY},
                {"rotationZ", model.rotationZ},
                {"scaleX", model.scaleX},
                {"scaleY", model.scaleY},
                {"scaleZ", model.scaleZ},
                {"renderWidth", model.renderWidth},
                {"renderHeight", model.renderHeight},
                {"renderDepth", model.renderDepth},
                {"supportedRenderStyles", model.supportedRenderStyles},
                {"controllerCalibration", {
                    {"controllerName", optionalJson(model.controllerCalibration.controllerName)},
                    {"controllerPort", optionalJson(model.controllerCalibration.controllerPort)},
                    {"connectionSource", model.controllerCalibration.connectionSource},
                    {"configuredBrightnessPercent", optionalJson(model.controllerCalibration.configuredBrightnessPercent)},
                    {"brightnessExplicitlySet", model.controllerCalibration.brightnessExplicitlySet},
                    {"brightnessActive", model.controllerCalibration.brightnessActive},
                    {"configuredBrightnessSource", model.controllerCalibration.configuredBrightnessSource},
                    {"configuredGamma", optionalJson(model.controllerCalibration.configuredGamma)},
                    {"gammaExplicitlySet", model.controllerCalibration.gammaExplicitlySet},
                    {"gammaActive", model.controllerCalibration.gammaActive},
                    {"configuredGammaSource", model.controllerCalibration.configuredGammaSource},
                    {"fullXlightsControlActive", optionalJson(model.controllerCalibration.fullXlightsControlActive)},
                    {"controllerDefaultBrightnessPercent", optionalJson(model.controllerCalibration.controllerDefaultBrightnessPercent)},
                    {"controllerDefaultBrightnessSource", model.controllerCalibration.controllerDefaultBrightnessSource},
                    {"controllerDefaultGamma", optionalJson(model.controllerCalibration.controllerDefaultGamma)},
                    {"controllerDefaultGammaSource", model.controllerCalibration.controllerDefaultGammaSource},
                    {"effectiveBrightnessPercent", optionalJson(model.controllerCalibration.effectiveBrightnessPercent)},
                    {"effectiveBrightnessSource", model.controllerCalibration.effectiveBrightnessSource},
                    {"effectiveGamma", optionalJson(model.controllerCalibration.effectiveGamma)},
                    {"effectiveGammaSource", model.controllerCalibration.effectiveGammaSource},
                    {"deploymentMetadataOnly", model.controllerCalibration.deploymentMetadataOnly},
                    {"previewApplicationProven", model.controllerCalibration.previewApplicationProven}
                }}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetScene(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        // Scene packages xLights preview settings with model transforms so the
        // designer can generate against the same display frame of reference.
        const auto summary = _service.getModels();
        const auto settings = _service.getSettings();
        response.data["models"] = nlohmann::json::array();
        response.data["preview"] = {
            {"name", settings.activePreview},
            {"mode", settings.preview3d ? "3d" : "2d"},
            {"canvasWidth", settings.previewWidth},
            {"canvasHeight", settings.previewHeight},
            {"virtualCanvasWidth", settings.virtualCanvasWidth},
            {"virtualCanvasHeight", settings.virtualCanvasHeight},
            {"display2dCenter0", settings.display2dCenter0},
            {"zoom", settings.previewZoom},
            {"alignment", {
                {"status", "unverified"},
                {"mode", "xlights_preview_contract_partial"},
                {"reason", "Owned API exposes preview construction settings, but target renderer has not yet reproduced xLights ModelPreview transforms."}
            }}
        };
        response.data["background"] = {
            {"imagePath", settings.backgroundImage},
            {"available", settings.backgroundImageAvailable},
            {"scaled", settings.backgroundScaled},
            {"brightness", settings.backgroundBrightness},
            {"alpha", settings.backgroundAlpha},
            {"composite", false},
            {"alignment", {
                {"status", "unverified"},
                {"reason", "Background image path and scale settings are exposed, but pixel alignment has not been verified."}
            }}
        };
        response.data["cameras"] = nlohmann::json::array();
        response.data["cameras"].push_back({
            {"id", settings.preview3d ? "layout-preview-3d-current" : "layout-preview-2d-current"},
            {"mode", settings.preview3d ? "3d" : "2d"},
            {"zoom", settings.previewZoom},
            {"status", "partial"}
        });
        response.data["views"] = nlohmann::json::array();
        response.data["displayElements"] = nlohmann::json::array();
        for (const auto& model : summary.models) {
            response.data["models"].push_back({
                {"name", model.name},
                {"type", model.displayAs},
                {"layoutGroup", model.layoutGroup},
                {"startChannel", model.startChannel},
                {"endChannel", model.endChannel},
                {"transform", {
                    {"position", {
                        {"x", model.positionX},
                        {"y", model.positionY},
                        {"z", model.positionZ}
                    }},
                    {"rotationDeg", {
                        {"x", model.rotationX},
                        {"y", model.rotationY},
                        {"z", model.rotationZ}
                    }},
                    {"scale", {
                        {"x", model.scaleX},
                        {"y", model.scaleY},
                        {"z", model.scaleZ}
                    }}
                }},
                {"dimensions", {
                    {"width", model.width},
                    {"height", model.height},
                    {"depth", model.depth}
                }},
                {"renderSize", {
                    {"width", model.renderWidth},
                    {"height", model.renderHeight},
                    {"depth", model.renderDepth}
                }},
                {"attributes", {
                    {"stringType", model.stringType},
                    {"submodelCount", model.submodelCount},
                    {"nodeCount", model.nodeCount},
                    {"startChannel", model.startChannel},
                    {"endChannel", model.endChannel}
                }}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetSubmodels(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getSubmodels();
        response.data["submodels"] = nlohmann::json::array();
        for (const auto& submodel : summary.submodels) {
            response.data["submodels"].push_back({
                {"name", submodel.name},
                {"fullName", submodel.fullName},
                {"parentName", submodel.parentName},
                {"layoutGroup", submodel.layoutGroup},
                {"layout", submodel.layout},
                {"type", submodel.type},
                {"bufferStyle", submodel.bufferStyle},
                {"lines", submodel.lines},
                {"nodeIds", submodel.nodeIds},
                {"nodeRows", nlohmann::json::array()},
                {"supportedRenderStyles", submodel.supportedRenderStyles},
                {"startChannel", submodel.startChannel},
                {"endChannel", submodel.endChannel},
                {"nodeCount", submodel.nodeCount},
                {"vertical", submodel.vertical},
                {"ranges", submodel.ranges}
            });
            auto& jsonSubmodel = response.data["submodels"].back();
            for (const auto& row : submodel.nodeRows) {
                jsonSubmodel["nodeRows"].push_back({
                    {"rowId", row.rowId},
                    {"label", row.label},
                    {"rawLine", row.rawLine},
                    {"order", row.order},
                    {"nodeIds", row.nodeIds}
                });
            }
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetModelNodes(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        // Node coordinates are optional by coordinate system to keep large
        // displays from returning data the caller does not need.
        models::LayoutModelNodesRequest nodesRequest;
        nodesRequest.name = parsing::ReadString(request.params, "name");
        nodesRequest.includeBufferCoords = parsing::ReadBool(request.params, "includeBufferCoords", true);
        nodesRequest.includeWorldCoords = parsing::ReadBool(request.params, "includeWorldCoords", true);
        nodesRequest.includeScreenCoords = parsing::ReadBool(request.params, "includeScreenCoords", false);

        if (nodesRequest.name.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"BAD_REQUEST", "layout.getModelNodes requires name.", nlohmann::json::object()};
            return response;
        }

        const auto summary = _service.getModelNodes(nodesRequest);
        if (!summary.found) {
            response.statusCode = 404;
            response.error = transport::ApiError{"MODEL_NOT_FOUND", "Model not found.", nlohmann::json{{"name", nodesRequest.name}}};
            return response;
        }

        response.data["modelName"] = summary.modelName;
        response.data["nodes"] = nlohmann::json::array();
        response.data["source"] = {
            {"isCustomModel", summary.isCustomModel}
        };
        response.data["requested"] = {
            {"includeBufferCoords", summary.includeBufferCoords},
            {"includeWorldCoords", summary.includeWorldCoords},
            {"includeScreenCoords", summary.includeScreenCoords}
        };
        for (const auto& node : summary.nodes) {
            nlohmann::json nodeJson{
                {"nodeId", node.nodeId},
                {"stringIndex", node.stringIndex},
                {"coords", nlohmann::json::array()}
            };
            if (!node.name.empty()) {
                nodeJson["name"] = node.name;
            }
            for (const auto& coord : node.coords) {
                nlohmann::json coordJson = nlohmann::json::object();
                if (coord.bufferX.has_value() && coord.bufferY.has_value()) {
                    coordJson["buffer"] = {
                        {"x", *coord.bufferX},
                        {"y", *coord.bufferY}
                    };
                }
                if (coord.worldX.has_value() && coord.worldY.has_value() && coord.worldZ.has_value()) {
                    coordJson["world"] = {
                        {"x", *coord.worldX},
                        {"y", *coord.worldY},
                        {"z", *coord.worldZ}
                    };
                }
                if (coord.screenX.has_value() && coord.screenY.has_value() && coord.screenZ.has_value()) {
                    coordJson["screen"] = {
                        {"x", *coord.screenX},
                        {"y", *coord.screenY},
                        {"z", *coord.screenZ}
                    };
                }
                nodeJson["coords"].push_back(std::move(coordJson));
            }
            response.data["nodes"].push_back(std::move(nodeJson));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetRenderBufferNodes(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        // Render-buffer coordinates are materialized by xLights for the exact
        // style/camera/transform tuple the render board will use.
        models::LayoutRenderBufferNodesRequest nodesRequest;
        nodesRequest.targetName = parsing::ReadString(request.params, "target", parsing::ReadString(request.params, "name"));
        nodesRequest.renderStyle = parsing::ReadString(request.params, "renderStyle", "Default");
        nodesRequest.camera = parsing::ReadString(request.params, "camera", "2D");
        nodesRequest.transform = parsing::ReadString(request.params, "transform", "None");
        nodesRequest.stagger = parsing::ReadInt(request.params, "stagger", 0);
        nodesRequest.deep = parsing::ReadBool(request.params, "deep", false);

        if (nodesRequest.targetName.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"BAD_REQUEST", "layout.getRenderBufferNodes requires target.", nlohmann::json::object()};
            return response;
        }

        const auto summary = _service.getRenderBufferNodes(nodesRequest);
        if (!summary.found) {
            response.statusCode = 404;
            response.error = transport::ApiError{"TARGET_NOT_FOUND", "Layout target not found.", nlohmann::json{{"target", nodesRequest.targetName}}};
            return response;
        }

        response.data["targetName"] = summary.targetName;
        response.data["requestedRenderStyle"] = summary.requestedRenderStyle;
        response.data["adjustedRenderStyle"] = summary.adjustedRenderStyle;
        response.data["camera"] = summary.camera;
        response.data["transform"] = summary.transform;
        response.data["stagger"] = summary.stagger;
        response.data["deep"] = summary.deep;
        response.data["bufferWidth"] = summary.bufferWidth;
        response.data["bufferHeight"] = summary.bufferHeight;
        response.data["supportedRenderStyles"] = summary.supportedRenderStyles;
        response.data["warnings"] = summary.warnings;
        response.data["nodes"] = nlohmann::json::array();
        for (const auto& node : summary.nodes) {
            nlohmann::json nodeJson{
                {"nodeId", node.nodeId},
                {"nodeIndex", node.nodeIndex},
                {"stringIndex", node.stringIndex},
                {"name", node.name},
                {"parentModelName", node.parentModelName},
                {"channelStart", node.channelStart},
                {"channelStartZeroBased", node.channelStartZeroBased},
                {"channelCount", node.channelCount},
                {"coords", nlohmann::json::array()}
            };
            for (const auto& coord : node.coords) {
                nlohmann::json coordJson = nlohmann::json::object();
                if (coord.bufferX.has_value() && coord.bufferY.has_value()) {
                    coordJson["buffer"] = {
                        {"x", *coord.bufferX},
                        {"y", *coord.bufferY}
                    };
                }
                nodeJson["coords"].push_back(std::move(coordJson));
            }
            response.data["nodes"].push_back(std::move(nodeJson));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetSettings(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto settings = _service.getSettings();
        response.data["hasUnsavedLayoutChanges"] = settings.hasUnsavedLayoutChanges;
        response.data["hasUnsavedRgbEffectsChanges"] = settings.hasUnsavedRgbEffectsChanges;
        response.data["hasUnsavedNetworkChanges"] = settings.hasUnsavedNetworkChanges;
        response.data["modelsChangeCount"] = settings.modelsChangeCount;
        response.data["previewWidth"] = settings.previewWidth;
        response.data["previewHeight"] = settings.previewHeight;
        response.data["virtualCanvasWidth"] = settings.virtualCanvasWidth;
        response.data["virtualCanvasHeight"] = settings.virtualCanvasHeight;
        response.data["preview3d"] = settings.preview3d;
        response.data["display2dCenter0"] = settings.display2dCenter0;
        response.data["previewZoom"] = settings.previewZoom;
        response.data["activePreview"] = settings.activePreview;
        response.data["backgroundImage"] = settings.backgroundImage;
        response.data["backgroundImageAvailable"] = settings.backgroundImageAvailable;
        response.data["backgroundScaled"] = settings.backgroundScaled;
        response.data["backgroundBrightness"] = settings.backgroundBrightness;
        response.data["backgroundAlpha"] = settings.backgroundAlpha;
        response.data["showDirectory"] = settings.showDirectory;
        response.data["rgbEffectsFile"] = settings.rgbEffectsFile;
        response.data["rgbEffectsModifiedAt"] = settings.rgbEffectsModifiedAt;
        response.data["networksFile"] = settings.networksFile;
        response.data["networksModifiedAt"] = settings.networksModifiedAt;
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetChannelMap(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getChannelMap();
        response.data["snapshotId"] = summary.snapshotId;
        response.data["fingerprint"] = summary.fingerprint;
        response.data["mappingEvidence"] = summary.mappingEvidence;
        response.data["maxChannelCount"] = summary.maxChannelCount;
        response.data["usableForFseq"] = summary.usableForFseq;
        response.data["warnings"] = summary.warnings;
        response.data["targets"] = nlohmann::json::array();
        for (const auto& target : summary.targets) {
            nlohmann::json targetJson{
                {"targetName", target.targetName},
                {"targetKind", target.targetKind},
                {"displayAs", target.displayAs},
                {"startChannel", target.startChannel},
                {"endChannel", target.endChannel},
                {"nodeCount", target.nodeCount},
                {"usableForFseq", target.usableForFseq},
                {"warnings", target.warnings},
                {"nodes", nlohmann::json::array()}
            };
            for (const auto& node : target.nodes) {
                targetJson["nodes"].push_back({
                    {"nodeId", node.nodeId},
                    {"nodeIndex", node.nodeIndex},
                    {"stringIndex", node.stringIndex},
                    {"name", node.name},
                    {"channelStart", node.channelStart},
                    {"channelStartZeroBased", node.channelStartZeroBased},
                    {"channelCount", node.channelCount},
                    {"evidence", node.evidence}
                });
            }
            response.data["targets"].push_back(std::move(targetJson));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetGroupMembers(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getGroupMemberships();
        response.data["groups"] = nlohmann::json::array();
        for (const auto& group : summary.groups) {
            response.data["groups"].push_back({
                {"groupName", group.groupName},
                {"directMembers", serializeMembers(group.directMembers)},
                {"activeMembers", serializeMembers(group.activeMembers)},
                {"flattenedMembers", serializeMembers(group.flattenedMembers)},
                {"flattenedAllMembers", serializeMembers(group.flattenedAllMembers)}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetPreviewGroups(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getPreviewGroups();
        response.data["previewGroups"] = nlohmann::json::array();
        for (const auto& group : summary.previewGroups) {
            response.data["previewGroups"].push_back({
                {"name", group.name},
                {"systemGroup", group.systemGroup},
                {"unassigned", group.unassigned},
                {"modelCount", group.modelCount},
                {"modelNames", group.modelNames}
            });
        }
        return response;
    }

private:
    static double readDouble(const std::map<std::string, std::string>& params, const char* key, double fallback) {
        const auto it = params.find(key);
        if (it == params.end() || it->second.empty()) return fallback;
        try {
            return std::stod(it->second);
        } catch (...) {
            return fallback;
        }
    }

    static nlohmann::json serializeMembers(const std::vector<models::LayoutGroupMemberSummary>& members) {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& member : members) {
            rows.push_back({
                {"id", member.id},
                {"name", member.name},
                {"type", member.type},
                {"isGroup", member.isGroup},
                {"isSubmodel", member.isSubmodel},
                {"active", member.active}
            });
        }
        return rows;
    }

    services::LayoutService _service;
};

} // namespace xLightsDesigner::api::handlers
