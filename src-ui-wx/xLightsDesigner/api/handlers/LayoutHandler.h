#pragma once

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

    [[nodiscard]] transport::ApiResponse handleGetSettings(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto settings = _service.getSettings();
        response.data["hasUnsavedLayoutChanges"] = settings.hasUnsavedLayoutChanges;
        response.data["hasUnsavedRgbEffectsChanges"] = settings.hasUnsavedRgbEffectsChanges;
        response.data["hasUnsavedNetworkChanges"] = settings.hasUnsavedNetworkChanges;
        response.data["modelsChangeCount"] = settings.modelsChangeCount;
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

private:
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
