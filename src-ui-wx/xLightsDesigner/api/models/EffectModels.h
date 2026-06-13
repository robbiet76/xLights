#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// Native xLights effects are controlled through their existing effect name,
// settings string, palette string, layer, and timing fields. XLD ownership is
// stored inside the effect settings as XLD_OWNER/XLD_ID metadata so user-owned
// effects can be read without being overwritten by default.
struct NativeEffectSummary {
    std::string id;
    std::string elementName;
    int layerIndex = 0;
    int effectIndex = 0;
    int nativeId = 0;
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
    std::string settings;
    std::string palette;
    bool protectedEffect = false;
    bool locked = false;
    bool renderDisabled = false;
    bool xldOwned = false;
    std::string xldId;
    std::string xldOwner;
};

struct NativeEffectListRequest {
    std::string elementName;
    std::optional<int> startMs;
    std::optional<int> endMs;
    bool xldOnly = false;
    bool includeSettings = true;
};

struct NativeEffectListResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    std::vector<NativeEffectSummary> effects;
};

struct NativeEffectLayerSummary {
    std::string elementName;
    int layerIndex = 0;
    int layerNumber = 0;
    std::string layerName;
    int effectCount = 0;
    bool hasUserOwnedEffects = false;
    bool hasXldOwnedEffects = false;
};

struct NativeEffectLayerListRequest {
    std::string elementName;
};

struct NativeEffectLayerListResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    std::vector<NativeEffectLayerSummary> layers;
};

struct NativeEffectLayerEnsureRequest {
    std::string elementName;
    int layerIndex = -1;
    std::string layerName;
};

struct NativeEffectLayerRemoveRequest {
    std::string elementName;
    int layerIndex = -1;
    bool allowUserOwned = false;
};

struct NativeEffectLayerMutationResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool layerFound = false;
    bool ok = false;
    bool created = false;
    bool removed = false;
    std::string errorCode;
    std::string errorMessage;
    NativeEffectLayerSummary layer;
};

struct NativeEffectUpsertRequest {
    std::string elementName;
    int layerIndex = 0;
    int nativeId = -1;
    std::string xldId;
    std::string xldOwner = "xLightsDesigner";
    std::string effectName;
    int startMs = 0;
    int endMs = 0;
    std::string settings;
    std::string palette;
    bool replaceExistingXld = true;
};

struct NativeEffectMutationResult {
    bool sequenceOpen = false;
    bool elementFound = false;
    bool layerFound = false;
    bool ok = false;
    bool created = false;
    bool updated = false;
    bool removed = false;
    std::string errorCode;
    std::string errorMessage;
    NativeEffectSummary effect;
};

struct NativeEffectRemoveRequest {
    std::string elementName;
    int layerIndex = 0;
    int nativeId = -1;
    std::string xldId;
    bool allowUserOwned = false;
};

} // namespace xLightsDesigner::api::models
