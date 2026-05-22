#pragma once

#include <functional>

#include "../models/SequenceModels.h"

namespace xLightsDesigner::api::services {

class SequenceService {
public:
    using ReadOpenSequenceFn = std::function<models::SequenceSummary()>;
    using ReadSequenceSettingsFn = std::function<models::SequenceSettings()>;
    using OpenSequenceFn = std::function<models::SequenceOpenResult(const models::SequenceOpenRequest&)>;
    using CreateSequenceFn = std::function<models::SequenceCreateResult(const models::SequenceCreateRequest&)>;
    using UpdateSequenceSettingsFn = std::function<models::SequenceSettingsUpdateResult(const models::SequenceSettingsUpdateRequest&)>;
    using SaveSequenceFn = std::function<models::SequenceSaveResult()>;
    using CloseSequenceFn = std::function<models::SequenceCloseResult()>;
    using FocusSequenceFn = std::function<models::SequenceFocusResult()>;
    using RenderSequenceFn = std::function<models::SequenceRenderResult()>;
    using CheckSequenceFn = std::function<models::SequenceCheckResult()>;
    using ExportPreviewVideoFn = std::function<models::SequencePreviewVideoExportResult(const models::SequencePreviewVideoExportRequest&)>;
    using ReadRenderedSamplesFn = std::function<models::SequenceRenderSamplesResult(const models::SequenceRenderSamplesRequest&)>;
    using ReadFinalFseqStateFn = std::function<models::SequenceFinalFseqState()>;
    using ReadSyncHealthFn = std::function<models::SequenceSyncHealthSummary()>;

    SequenceService(ReadOpenSequenceFn readOpenSequence,
                    ReadSequenceSettingsFn readSequenceSettings,
                    OpenSequenceFn openSequence,
                    CreateSequenceFn createSequence,
                    UpdateSequenceSettingsFn updateSequenceSettings,
                    SaveSequenceFn saveSequence,
                    CloseSequenceFn closeSequence,
                    FocusSequenceFn focusSequence,
                    RenderSequenceFn renderSequence,
                    CheckSequenceFn checkSequence,
                    ExportPreviewVideoFn exportPreviewVideo,
                    ReadRenderedSamplesFn readRenderedSamples,
                    ReadFinalFseqStateFn readFinalFseqState,
                    ReadSyncHealthFn readSyncHealth)
        : _readOpenSequence(std::move(readOpenSequence)),
          _readSequenceSettings(std::move(readSequenceSettings)),
          _openSequence(std::move(openSequence)),
          _createSequence(std::move(createSequence)),
          _updateSequenceSettings(std::move(updateSequenceSettings)),
          _saveSequence(std::move(saveSequence)),
          _closeSequence(std::move(closeSequence)),
          _focusSequence(std::move(focusSequence)),
          _renderSequence(std::move(renderSequence)),
          _checkSequence(std::move(checkSequence)),
          _exportPreviewVideo(std::move(exportPreviewVideo)),
          _readRenderedSamples(std::move(readRenderedSamples)),
          _readFinalFseqState(std::move(readFinalFseqState)),
          _readSyncHealth(std::move(readSyncHealth)) {}

    [[nodiscard]] models::SequenceSummary getOpenSequence() const {
        return _readOpenSequence ? _readOpenSequence() : models::SequenceSummary{};
    }

    [[nodiscard]] models::SequenceSummary getRevision() const {
        return getOpenSequence();
    }

    [[nodiscard]] models::SequenceSettings getSettings() const {
        return _readSequenceSettings ? _readSequenceSettings() : models::SequenceSettings{};
    }

    [[nodiscard]] models::SequenceOpenResult openSequence(const models::SequenceOpenRequest& request) const {
        return _openSequence ? _openSequence(request) : models::SequenceOpenResult{};
    }

    [[nodiscard]] models::SequenceCreateResult createSequence(const models::SequenceCreateRequest& request) const {
        return _createSequence ? _createSequence(request) : models::SequenceCreateResult{};
    }

    [[nodiscard]] models::SequenceSettingsUpdateResult updateSettings(const models::SequenceSettingsUpdateRequest& request) const {
        return _updateSequenceSettings ? _updateSequenceSettings(request) : models::SequenceSettingsUpdateResult{};
    }

    [[nodiscard]] models::SequenceSaveResult saveSequence() const {
        return _saveSequence ? _saveSequence() : models::SequenceSaveResult{};
    }

    [[nodiscard]] models::SequenceCloseResult closeSequence() const {
        return _closeSequence ? _closeSequence() : models::SequenceCloseResult{};
    }

    [[nodiscard]] models::SequenceFocusResult focusSequence() const {
        return _focusSequence ? _focusSequence() : models::SequenceFocusResult{};
    }

    [[nodiscard]] models::SequenceRenderResult renderSequence() const {
        return _renderSequence ? _renderSequence() : models::SequenceRenderResult{};
    }

    [[nodiscard]] models::SequenceCheckResult checkSequence() const {
        return _checkSequence ? _checkSequence() : models::SequenceCheckResult{};
    }

    [[nodiscard]] models::SequencePreviewVideoExportResult exportPreviewVideo(const models::SequencePreviewVideoExportRequest& request) const {
        return _exportPreviewVideo ? _exportPreviewVideo(request) : models::SequencePreviewVideoExportResult{};
    }

    [[nodiscard]] models::SequenceRenderSamplesResult getRenderedSamples(const models::SequenceRenderSamplesRequest& request) const {
        return _readRenderedSamples ? _readRenderedSamples(request) : models::SequenceRenderSamplesResult{};
    }

    [[nodiscard]] models::SequenceFinalFseqState getFinalFseqState() const {
        return _readFinalFseqState ? _readFinalFseqState() : models::SequenceFinalFseqState{};
    }

    [[nodiscard]] models::SequenceSyncHealthSummary getSyncHealth() const {
        return _readSyncHealth ? _readSyncHealth() : models::SequenceSyncHealthSummary{};
    }

private:
    ReadOpenSequenceFn _readOpenSequence;
    ReadSequenceSettingsFn _readSequenceSettings;
    OpenSequenceFn _openSequence;
    CreateSequenceFn _createSequence;
    UpdateSequenceSettingsFn _updateSequenceSettings;
    SaveSequenceFn _saveSequence;
    CloseSequenceFn _closeSequence;
    FocusSequenceFn _focusSequence;
    RenderSequenceFn _renderSequence;
    CheckSequenceFn _checkSequence;
    ExportPreviewVideoFn _exportPreviewVideo;
    ReadRenderedSamplesFn _readRenderedSamples;
    ReadFinalFseqStateFn _readFinalFseqState;
    ReadSyncHealthFn _readSyncHealth;
};

} // namespace xLightsDesigner::api::services
