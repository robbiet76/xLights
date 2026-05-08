#pragma once

#include <exception>
#include <map>

#include "../parsing/ParameterReaders.h"
#include "../services/LayoutService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"

namespace xLightsDesigner::api::handlers {

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
            response.data["models"].push_back({
                {"name", model.name},
                {"displayAs", model.displayAs},
                {"stringType", model.stringType},
                {"layoutGroup", model.layoutGroup},
                {"startChannel", model.startChannel},
                {"endChannel", model.endChannel},
                {"submodelCount", model.submodelCount},
                {"nodeCount", model.nodeCount},
                {"positionX", model.positionX},
                {"positionY", model.positionY},
                {"positionZ", model.positionZ},
                {"width", model.width},
                {"height", model.height},
                {"depth", model.depth}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetScene(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getModels();
        response.data["models"] = nlohmann::json::array();
        response.data["cameras"] = nlohmann::json::array();
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
                        {"x", 0.0},
                        {"y", 0.0},
                        {"z", 0.0}
                    }},
                    {"scale", {
                        {"x", 1.0},
                        {"y", 1.0},
                        {"z", 1.0}
                    }}
                }},
                {"dimensions", {
                    {"width", model.width},
                    {"height", model.height},
                    {"depth", model.depth}
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
                {"startChannel", submodel.startChannel},
                {"endChannel", submodel.endChannel},
                {"nodeCount", submodel.nodeCount},
                {"vertical", submodel.vertical},
                {"ranges", submodel.ranges}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetModelNodes(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

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
            {"isCustomModel", summary.isCustomModel},
            {"customModelParsed", summary.isCustomModel}
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
        response.data["showDirectory"] = settings.showDirectory;
        response.data["rgbEffectsFile"] = settings.rgbEffectsFile;
        response.data["rgbEffectsModifiedAt"] = settings.rgbEffectsModifiedAt;
        response.data["networksFile"] = settings.networksFile;
        response.data["networksModifiedAt"] = settings.networksModifiedAt;
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

    [[nodiscard]] transport::ApiResponse handleCreateCustomModel(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::CreateCustomModelRequest createRequest;
        createRequest.name = parsing::ReadString(request.params, "name");
        createRequest.startChannel = parsing::ReadString(request.params, "startChannel", "1");
        createRequest.layoutGroup = parsing::ReadString(request.params, "layoutGroup", "Default");
        createRequest.width = parsing::ReadInt(request.params, "width", 0);
        createRequest.height = parsing::ReadInt(request.params, "height", 0);
        createRequest.depth = parsing::ReadInt(request.params, "depth", 1);
        createRequest.stringCount = parsing::ReadInt(request.params, "stringCount", 1);
        createRequest.positionX = readDouble(request.params, "positionX", 0.0);
        createRequest.positionY = readDouble(request.params, "positionY", 0.0);
        createRequest.overwrite = parsing::ReadBool(request.params, "overwrite", false);
        const bool dryRun = request.dryRun || parsing::ReadBool(request.params, "dryRun", false);
        createRequest.dryRun = dryRun;

        const auto nodesJsonText = parsing::ReadString(request.params, "nodes");
        if (nodesJsonText.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{"BAD_REQUEST", "layout.createCustomModel requires nodes.", nlohmann::json::object()};
            return response;
        }

        try {
            const auto nodes = nlohmann::json::parse(nodesJsonText);
            if (!nodes.is_array()) {
                response.statusCode = 400;
                response.error = transport::ApiError{"BAD_REQUEST", "layout.createCustomModel nodes must be an array.", nlohmann::json::object()};
                return response;
            }
            for (const auto& row : nodes) {
                models::CustomModelNode node;
                node.x = row.value("x", -1);
                node.y = row.value("y", -1);
                node.z = row.value("z", 0);
                node.node = row.value("node", -1);
                node.string = row.value("string", 1);
                createRequest.nodes.push_back(node);
            }
        } catch (const std::exception& ex) {
            response.statusCode = 400;
            response.error = transport::ApiError{"BAD_REQUEST", std::string("Invalid custom model nodes JSON: ") + ex.what(), nlohmann::json::object()};
            return response;
        }

        const auto result = _service.createCustomModel(createRequest);
        if (!result.created && !result.updated) {
            if (dryRun && result.errorMessage.empty()) {
                response.data["created"] = false;
                response.data["updated"] = false;
                response.data["dryRun"] = true;
                response.data["modelName"] = result.modelName;
                response.data["nodeCount"] = result.nodeCount;
                response.data["width"] = result.width;
                response.data["height"] = result.height;
                response.data["depth"] = result.depth;
                return response;
            }
            response.statusCode = 409;
            response.error = transport::ApiError{"CUSTOM_MODEL_NOT_CREATED", result.errorMessage.empty() ? "Custom model could not be created." : result.errorMessage, nlohmann::json::object()};
            return response;
        }
        response.data["created"] = result.created;
        response.data["updated"] = result.updated;
        response.data["modelName"] = result.modelName;
        response.data["nodeCount"] = result.nodeCount;
        response.data["width"] = result.width;
        response.data["height"] = result.height;
        response.data["depth"] = result.depth;
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
