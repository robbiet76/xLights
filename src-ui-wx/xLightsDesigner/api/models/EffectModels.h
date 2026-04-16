#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

struct EffectWindowRequest {
    std::string elementName;
    int startMs = 0;
    int endMs = 0;
};

struct EffectWindowEntry {
    int layerNumber = 0;
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
};

struct EffectWindowSummary {
    bool sequenceOpen = false;
    bool elementFound = false;
    std::string elementName;
    int startMs = 0;
    int endMs = 0;
    std::vector<EffectWindowEntry> effects;
};

struct AddEffectRequest {
    std::string elementName;
    int layerNumber = 0;
    std::string effectName;
    std::string settings;
    std::string palette;
    int startMs = 0;
    int endMs = 0;
};

struct AddEffectResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool created = false;
    std::string elementName;
    int layerNumber = 0;
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
};

struct ClearEffectWindowRequest {
    std::string elementName;
    int layerNumber = 0;
    int startMs = 0;
    int endMs = 0;
};

struct ClearEffectWindowResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool layerFound = false;
    std::string elementName;
    int layerNumber = 0;
    int startMs = 0;
    int endMs = 0;
    int clearedEffectCount = 0;
};

struct EffectBatchItemRequest {
    std::string elementName;
    int layerNumber = 0;
    std::string effectName;
    std::string settings;
    std::string palette;
    int startMs = 0;
    int endMs = 0;
    bool clearExisting = false;
};

struct ApplyEffectBatchRequest {
    std::vector<EffectBatchItemRequest> effects;
};

struct EffectBatchItemResult {
    std::string elementName;
    int layerNumber = 0;
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
    bool clearExisting = false;
    int clearedEffectCount = 0;
    bool created = false;
};

struct ApplyEffectBatchResult {
    bool sequenceOpen = false;
    bool ok = false;
    int requestedCount = 0;
    int createdCount = 0;
    int clearedEffectCount = 0;
    std::optional<int> failedItemIndex;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    std::vector<EffectBatchItemResult> items;
};

} // namespace xLightsDesigner::api::models
