#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// Element models expose sequence row inventory, display order, and sequencer
// selection state. Native effect mutation lives in EffectModels.
struct ElementLayerSummary {
    int layerNumber = 0;
    int effectCount = 0;
    std::string layerName;
};

struct SequenceElementSummary {
    std::string name;
    std::string type;
    bool selected = false;
    bool visible = false;
    int totalEffectCount = 0;
    std::vector<ElementLayerSummary> layers;
};

struct ElementsSummary {
    bool sequenceOpen = false;
    std::vector<SequenceElementSummary> elements;
};

struct DisplayElementOrderEntry {
    std::string id;
    std::string type;
    int orderIndex = 0;
};

struct DisplayElementOrderSummary {
    bool sequenceOpen = false;
    std::vector<DisplayElementOrderEntry> elements;
};

struct SelectedDisplayElementsSummary {
    bool sequenceOpen = false;
    std::vector<std::string> selectedElementNames;
};

struct SetSelectedDisplayElementsRequest {
    std::vector<std::string> elementNames;
    bool replaceExisting = true;
};

struct SetSelectedDisplayElementsResult {
    bool sequenceOpen = false;
    bool ok = false;
    int selectedCount = 0;
    std::vector<std::string> missingNames;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct SetDisplayElementVisibilityRequest {
    std::vector<std::string> visibleElementNames;
    bool hideUnlistedModels = true;
};

struct SetDisplayElementVisibilityResult {
    bool sequenceOpen = false;
    bool ok = false;
    int visibleCount = 0;
    int hiddenCount = 0;
    std::vector<std::string> missingNames;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct SetDisplayElementOrderRequest {
    std::vector<std::string> orderedIds;
};

struct SetDisplayElementOrderResult {
    bool sequenceOpen = false;
    bool ok = false;
    int orderedCount = 0;
    std::vector<std::string> missingIds;
    std::vector<std::string> duplicateIds;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct EnsureSequenceElementsRequest {
    std::vector<std::string> elementNames;
};

struct EnsureSequenceElementsResult {
    bool sequenceOpen = false;
    bool ok = false;
    int addedCount = 0;
    std::vector<std::string> addedNames;
    std::vector<std::string> existingNames;
    std::vector<std::string> missingNames;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

} // namespace xLightsDesigner::api::models
