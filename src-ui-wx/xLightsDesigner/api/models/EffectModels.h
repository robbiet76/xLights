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
    int effectId = 0;
    int layerNumber = 0;
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
    std::string settings;
    std::string palette;
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

struct EffectSelectorRequest {
    std::string elementName;
    std::optional<int> effectId;
    std::optional<int> layerNumber;
    std::optional<int> startMs;
    std::optional<int> endMs;
    std::string effectName;
};

struct UpdateEffectRequest {
    EffectSelectorRequest selector;
    std::optional<int> layerNumber;
    std::optional<int> startMs;
    std::optional<int> endMs;
    std::optional<std::string> effectName;
    std::optional<std::string> settings;
    std::optional<std::string> palette;
};

struct UpdateEffectResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool ok = false;
    int matchedCount = 0;
    int updatedCount = 0;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct DeleteEffectsRequest {
    EffectSelectorRequest selector;
};

struct DeleteEffectsResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool ok = false;
    int deletedCount = 0;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct DeleteEffectLayerRequest {
    std::string elementName;
    int layerNumber = 0;
    bool force = false;
};

struct DeleteEffectLayerResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool layerFound = false;
    bool ok = false;
    int layerNumber = 0;
    int layerCount = 0;
    int effectCount = 0;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct ReorderEffectLayerRequest {
    std::string elementName;
    int fromLayer = 0;
    int toLayer = 0;
};

struct ReorderEffectLayerResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool ok = false;
    int fromLayer = 0;
    int toLayer = 0;
    int layerCount = 0;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

struct CompactEffectLayersRequest {
    std::string elementName;
};

struct CompactEffectLayersResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool ok = false;
    int layerCount = 0;
    std::vector<int> removedLayerNumbers;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

} // namespace xLightsDesigner::api::models
