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
    using SaveSequenceFn = std::function<models::SequenceSaveResult()>;
    using CloseSequenceFn = std::function<models::SequenceCloseResult()>;
    using RenderSequenceFn = std::function<models::SequenceRenderResult()>;
    using ReadRenderedSamplesFn = std::function<models::SequenceRenderSamplesResult(const models::SequenceRenderSamplesRequest&)>;

    SequenceService(ReadOpenSequenceFn readOpenSequence,
                    ReadSequenceSettingsFn readSequenceSettings,
                    OpenSequenceFn openSequence,
                    CreateSequenceFn createSequence,
                    SaveSequenceFn saveSequence,
                    CloseSequenceFn closeSequence,
                    RenderSequenceFn renderSequence,
                    ReadRenderedSamplesFn readRenderedSamples)
        : _readOpenSequence(std::move(readOpenSequence)),
          _readSequenceSettings(std::move(readSequenceSettings)),
          _openSequence(std::move(openSequence)),
          _createSequence(std::move(createSequence)),
          _saveSequence(std::move(saveSequence)),
          _closeSequence(std::move(closeSequence)),
          _renderSequence(std::move(renderSequence)),
          _readRenderedSamples(std::move(readRenderedSamples)) {}

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

    [[nodiscard]] models::SequenceSaveResult saveSequence() const {
        return _saveSequence ? _saveSequence() : models::SequenceSaveResult{};
    }

    [[nodiscard]] models::SequenceCloseResult closeSequence() const {
        return _closeSequence ? _closeSequence() : models::SequenceCloseResult{};
    }

    [[nodiscard]] models::SequenceRenderResult renderSequence() const {
        return _renderSequence ? _renderSequence() : models::SequenceRenderResult{};
    }

    [[nodiscard]] models::SequenceRenderSamplesResult getRenderedSamples(const models::SequenceRenderSamplesRequest& request) const {
        return _readRenderedSamples ? _readRenderedSamples(request) : models::SequenceRenderSamplesResult{};
    }

private:
    ReadOpenSequenceFn _readOpenSequence;
    ReadSequenceSettingsFn _readSequenceSettings;
    OpenSequenceFn _openSequence;
    CreateSequenceFn _createSequence;
    SaveSequenceFn _saveSequence;
    CloseSequenceFn _closeSequence;
    RenderSequenceFn _renderSequence;
    ReadRenderedSamplesFn _readRenderedSamples;
};

} // namespace xLightsDesigner::api::services
