#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// Layout models describe the existing xLights display skeleton. This API is
// read-only: callers use it to generate and map pixels/nodes, not to edit layout.
struct LayoutControllerCalibrationSummary {
    std::optional<std::string> controllerName;
    std::optional<int> controllerPort;
    std::string connectionSource = "unavailable";

    std::optional<int> configuredBrightnessPercent;
    bool brightnessExplicitlySet = false;
    bool brightnessActive = false;
    std::string configuredBrightnessSource = "unavailable";

    std::optional<double> configuredGamma;
    bool gammaExplicitlySet = false;
    bool gammaActive = false;
    std::string configuredGammaSource = "unavailable";

    std::optional<bool> fullXlightsControlActive;
    std::optional<int> controllerDefaultBrightnessPercent;
    std::string controllerDefaultBrightnessSource = "controller_not_resolved";
    std::optional<double> controllerDefaultGamma;
    std::string controllerDefaultGammaSource = "controller_not_resolved";

    std::optional<int> effectiveBrightnessPercent;
    std::string effectiveBrightnessSource = "unavailable";
    std::optional<double> effectiveGamma;
    std::string effectiveGammaSource = "unavailable";

    // These values describe upload/deployment configuration. The layout API
    // does not prove that xLights' preview renderer applies them.
    bool deploymentMetadataOnly = true;
    bool previewApplicationProven = false;
};

struct LayoutModelSummary {
    std::string name;
    std::string displayAs;
    std::string stringType;
    std::string layoutGroup;
    int startChannel = 0; // 1-based
    int endChannel = 0;   // 1-based inclusive
    int submodelCount = 0;
    int nodeCount = 0;
    int previewPixelSize = 2;
    double positionX = 0.0;
    double positionY = 0.0;
    double positionZ = 0.0;
    double width = 0.0;
    double height = 0.0;
    double depth = 0.0;
    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double scaleZ = 1.0;
    double renderWidth = 0.0;
    double renderHeight = 0.0;
    double renderDepth = 0.0;
    std::vector<std::string> supportedRenderStyles;
    LayoutControllerCalibrationSummary controllerCalibration;
};

struct LayoutModelsSummary {
    std::vector<LayoutModelSummary> models;
};

struct LayoutSubmodelRowSummary {
    std::string rowId;
    std::string label;
    std::string rawLine;
    std::vector<std::string> nodeIds;
    int order = 0;
};

struct LayoutSubmodelSummary {
    std::string name;
    std::string fullName;
    std::string parentName;
    std::string layoutGroup;
    std::string layout;
    std::string type;
    std::string bufferStyle;
    std::string lines;
    std::vector<std::string> nodeIds;
    std::vector<LayoutSubmodelRowSummary> nodeRows;
    std::vector<std::string> supportedRenderStyles;
    int startChannel = 0;
    int endChannel = 0;
    int nodeCount = 0;
    bool vertical = false;
    bool ranges = false;
};

struct LayoutSubmodelsSummary {
    std::vector<LayoutSubmodelSummary> submodels;
};

struct LayoutModelNodesRequest {
    std::string name;
    bool includeBufferCoords = true;
    bool includeWorldCoords = true;
    bool includeScreenCoords = false;
};

struct LayoutNodeCoordSummary {
    std::optional<int> bufferX;
    std::optional<int> bufferY;
    std::optional<double> worldX;
    std::optional<double> worldY;
    std::optional<double> worldZ;
    std::optional<double> screenX;
    std::optional<double> screenY;
    std::optional<double> screenZ;
};

struct LayoutModelNodeSummary {
    int nodeId = 0;
    int stringIndex = 0;
    std::string name;
    std::vector<LayoutNodeCoordSummary> coords;
};

struct LayoutModelNodesSummary {
    bool found = false;
    std::string modelName;
    bool isCustomModel = false;
    bool includeBufferCoords = true;
    bool includeWorldCoords = true;
    bool includeScreenCoords = false;
    std::vector<LayoutModelNodeSummary> nodes;
};

struct LayoutRenderBufferNodesRequest {
    std::string targetName;
    std::string renderStyle = "Default";
    std::string camera = "2D";
    std::string transform = "None";
    int stagger = 0;
    bool deep = false;
};

struct LayoutRenderBufferNodeSummary {
    int nodeId = 0;
    int nodeIndex = 0;
    int stringIndex = 0;
    std::string name;
    std::string parentModelName;
    int channelStart = 0; // 1-based
    int channelStartZeroBased = 0;
    int channelCount = 0;
    std::vector<LayoutNodeCoordSummary> coords;
};

struct LayoutRenderBufferNodesSummary {
    bool found = false;
    std::string targetName;
    std::string requestedRenderStyle;
    std::string adjustedRenderStyle;
    std::string camera;
    std::string transform;
    int stagger = 0;
    bool deep = false;
    int bufferWidth = 0;
    int bufferHeight = 0;
    std::vector<std::string> supportedRenderStyles;
    std::vector<LayoutRenderBufferNodeSummary> nodes;
    std::vector<std::string> warnings;
};

struct LayoutChannelMapNode {
    int nodeId = 0;
    int nodeIndex = 0;
    int stringIndex = 0;
    std::string name;
    int channelStart = 0; // 1-based
    int channelStartZeroBased = 0;
    int channelCount = 0;
    std::string evidence;
};

struct LayoutChannelMapTarget {
    std::string targetName;
    std::string targetKind = "model";
    std::string displayAs;
    int startChannel = 0; // 1-based
    int endChannel = 0;   // 1-based inclusive
    int nodeCount = 0;
    bool usableForFseq = false;
    std::vector<LayoutChannelMapNode> nodes;
    std::vector<std::string> warnings;
};

struct LayoutChannelMapSummary {
    std::string snapshotId;
    std::string fingerprint;
    std::string mappingEvidence;
    int maxChannelCount = 0;
    bool usableForFseq = false;
    std::vector<LayoutChannelMapTarget> targets;
    std::vector<std::string> warnings;
};

struct LayoutSettingsSummary {
    bool hasUnsavedLayoutChanges = false;
    bool hasUnsavedRgbEffectsChanges = false;
    bool hasUnsavedNetworkChanges = false;
    unsigned int modelsChangeCount = 0;
    int previewWidth = 0;
    int previewHeight = 0;
    int virtualCanvasWidth = 0;
    int virtualCanvasHeight = 0;
    bool preview3d = false;
    bool display2dCenter0 = false;
    double previewZoom = 1.0;
    std::string activePreview = "Default";
    std::string backgroundImage;
    bool backgroundImageAvailable = false;
    bool backgroundScaled = false;
    int backgroundBrightness = 100;
    int backgroundAlpha = 100;
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

struct LayoutPreviewGroupSummary {
    std::string name;
    bool systemGroup = false;
    bool unassigned = false;
    int modelCount = 0;
    std::vector<std::string> modelNames;
};

struct LayoutPreviewGroupsSummary {
    std::vector<LayoutPreviewGroupSummary> previewGroups;
};

} // namespace xLightsDesigner::api::models
