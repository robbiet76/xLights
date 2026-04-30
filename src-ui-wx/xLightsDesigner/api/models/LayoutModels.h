#pragma once

#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

struct LayoutModelSummary {
    std::string name;
    std::string displayAs;
    std::string stringType;
    std::string layoutGroup;
    int startChannel = 0; // 1-based
    int endChannel = 0;   // 1-based inclusive
    int submodelCount = 0;
    int nodeCount = 0;
    double positionX = 0.0;
    double positionY = 0.0;
    double positionZ = 0.0;
    double width = 0.0;
    double height = 0.0;
    double depth = 0.0;
};

struct LayoutModelsSummary {
    std::vector<LayoutModelSummary> models;
};

struct CustomModelNode {
    int x = -1;
    int y = -1;
    int z = 0;
    int node = -1;
    int string = 1;
};

struct CreateCustomModelRequest {
    std::string name;
    std::string startChannel = "1";
    std::string layoutGroup = "Default";
    int width = 0;
    int height = 0;
    int depth = 1;
    int stringCount = 1;
    double positionX = 0.0;
    double positionY = 0.0;
    bool overwrite = false;
    bool dryRun = false;
    std::vector<CustomModelNode> nodes;
};

struct CreateCustomModelResult {
    bool created = false;
    bool updated = false;
    std::string modelName;
    std::string errorMessage;
    int nodeCount = 0;
    int width = 0;
    int height = 0;
    int depth = 0;
};

struct LayoutSettingsSummary {
    bool hasUnsavedLayoutChanges = false;
    bool hasUnsavedRgbEffectsChanges = false;
    bool hasUnsavedNetworkChanges = false;
    unsigned int modelsChangeCount = 0;
    std::string showDirectory;
    std::string rgbEffectsFile;
    std::string rgbEffectsModifiedAt;
    std::string networksFile;
    std::string networksModifiedAt;
};

struct LayoutGroupMemberSummary {
    std::string id;
    std::string name;
    std::string type;
    bool isGroup = false;
    bool isSubmodel = false;
    bool active = false;
};

struct LayoutGroupMembersSummary {
    std::string groupName;
    std::vector<LayoutGroupMemberSummary> directMembers;
    std::vector<LayoutGroupMemberSummary> activeMembers;
    std::vector<LayoutGroupMemberSummary> flattenedMembers;
    std::vector<LayoutGroupMemberSummary> flattenedAllMembers;
};

struct LayoutGroupMembershipsSummary {
    std::vector<LayoutGroupMembersSummary> groups;
};

} // namespace xLightsDesigner::api::models
