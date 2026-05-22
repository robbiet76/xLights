#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// Element models expose sequence row inventory and display order without
// exposing native effect mutation.
struct ElementLayerSummary {
    int layerNumber = 0;
    int effectCount = 0;
    std::string layerName;
};

struct SequenceElementSummary {
    std::string name;
    std::string type;
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

} // namespace xLightsDesigner::api::models
