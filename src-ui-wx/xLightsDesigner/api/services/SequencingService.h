#pragma once

#include <utility>

#include "EffectService.h"
#include "TimingService.h"
#include "../models/SequencingModels.h"

namespace xLightsDesigner::api::services {

class SequencingService {
public:
    SequencingService(TimingService timingService,
                      EffectService effectService)
        : _timingService(std::move(timingService)),
          _effectService(std::move(effectService)) {}

    [[nodiscard]] models::SequencingApplyWindowPlanResult applyWindowPlan(
        const models::SequencingApplyWindowPlanRequest& request) const {
        models::SequencingApplyWindowPlanResult result;

        result.ensureTrack = _timingService.ensureTrack(request.ensureTrack);
        result.sequenceOpen = result.ensureTrack.sequenceOpen;
        if (!result.sequenceOpen) {
            return result;
        }

        if (result.ensureTrack.created && request.addMarks.marks.size() < 2) {
            result.errorCode = "FULL_TRACK_CONTEXT_REQUIRED";
            result.errorMessage = "Creating a new timing track requires the full section set, not a single mark.";
            result.warnings.push_back("New timing tracks must be populated with the full section context.");
            return result;
        }

        models::AddTimingMarksRequest addMarksRequest = request.addMarks;
        if (addMarksRequest.trackName.empty()) {
            addMarksRequest.trackName = result.ensureTrack.actualTrackName;
        }
        result.addMarks = _timingService.addMarks(addMarksRequest);
        if (!result.addMarks.sequenceOpen) {
            return result;
        }

        result.clearWindow = _effectService.clearWindow(request.clearWindow);
        if (!result.clearWindow.sequenceOpen || !result.clearWindow.elementFound || !result.clearWindow.layerFound) {
            return result;
        }

        result.addEffect = _effectService.addEffect(request.addEffect);
        if (!result.addEffect.sequenceOpen) {
            return result;
        }

        result.ok = result.addEffect.created;
        return result;
    }

    [[nodiscard]] models::SequencingApplyBatchPlanResult applyBatchPlan(
        const models::SequencingApplyBatchPlanRequest& request) const {
        models::SequencingApplyBatchPlanResult result;

        result.ensureTrack = _timingService.ensureTrack(request.ensureTrack);
        result.sequenceOpen = result.ensureTrack.sequenceOpen;
        if (!result.sequenceOpen) {
            return result;
        }

        if (result.ensureTrack.created && request.addMarks.marks.size() < 2) {
            result.errorCode = "FULL_TRACK_CONTEXT_REQUIRED";
            result.errorMessage = "Creating a new timing track requires the full section set, not a single mark.";
            result.warnings.push_back("New timing tracks must be populated with the full section context.");
            return result;
        }

        models::AddTimingMarksRequest addMarksRequest = request.addMarks;
        if (addMarksRequest.trackName.empty()) {
            addMarksRequest.trackName = result.ensureTrack.actualTrackName;
        }
        result.addMarks = _timingService.addMarks(addMarksRequest);
        if (!result.addMarks.sequenceOpen) {
            return result;
        }

        result.effectBatch = _effectService.applyBatch(request.effectBatch);
        if (!result.effectBatch.sequenceOpen) {
            return result;
        }
        if (!result.effectBatch.ok) {
            result.errorCode = result.effectBatch.errorCode.value_or(std::string("VALIDATION_ERROR"));
            result.errorMessage = result.effectBatch.errorMessage.value_or(std::string("sequencing.applyBatchPlan failed while applying effects."));
            return result;
        }

        result.ok = true;
        return result;
    }

private:
    TimingService _timingService;
    EffectService _effectService;
};

} // namespace xLightsDesigner::api::services
