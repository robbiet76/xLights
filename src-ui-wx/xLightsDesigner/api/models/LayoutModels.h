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
