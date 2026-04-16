#pragma once

#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

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

} // namespace xLightsDesigner::api::models
