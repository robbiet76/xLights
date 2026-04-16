#pragma once

#include <optional>
#include <string>
#include <vector>

#include "EffectModels.h"
#include "TimingModels.h"

namespace xLightsDesigner::api::models {

struct SequencingApplyWindowPlanRequest {
    EnsureTimingTrackRequest ensureTrack;
    AddTimingMarksRequest addMarks;
    ClearEffectWindowRequest clearWindow;
    AddEffectRequest addEffect;
};

struct SequencingApplyWindowPlanResult {
    bool sequenceOpen = false;
    bool ok = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    EnsureTimingTrackResult ensureTrack;
    AddTimingMarksResult addMarks;
    ClearEffectWindowResult clearWindow;
    AddEffectResult addEffect;
    std::vector<std::string> warnings;
};

struct SequencingApplyBatchPlanRequest {
    EnsureTimingTrackRequest ensureTrack;
    AddTimingMarksRequest addMarks;
    ApplyEffectBatchRequest effectBatch;
};

struct SequencingApplyBatchPlanResult {
    bool sequenceOpen = false;
    bool ok = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    EnsureTimingTrackResult ensureTrack;
    AddTimingMarksResult addMarks;
    ApplyEffectBatchResult effectBatch;
    std::vector<std::string> warnings;
};

} // namespace xLightsDesigner::api::models
