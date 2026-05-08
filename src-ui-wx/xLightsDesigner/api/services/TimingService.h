#pragma once

#include <functional>

#include "../models/TimingModels.h"

namespace xLightsDesigner::api::services {

class TimingService {
public:
    using ReadTimingTracksFn = std::function<models::TimingTracksSummary()>;
    using ReadTimingMarksFn = std::function<models::TimingMarksSummary(const models::TimingMarksRequest&)>;
    using EnsureTimingTrackFn = std::function<models::EnsureTimingTrackResult(const models::EnsureTimingTrackRequest&)>;
    using AddTimingMarksFn = std::function<models::AddTimingMarksResult(const models::AddTimingMarksRequest&)>;

    TimingService(ReadTimingTracksFn readTimingTracks,
                  ReadTimingMarksFn readTimingMarks,
                  EnsureTimingTrackFn ensureTimingTrack,
                  AddTimingMarksFn addTimingMarks)
        : _readTimingTracks(std::move(readTimingTracks)),
          _readTimingMarks(std::move(readTimingMarks)),
          _ensureTimingTrack(std::move(ensureTimingTrack)),
          _addTimingMarks(std::move(addTimingMarks)) {}

    [[nodiscard]] models::TimingTracksSummary getTracks() const {
        return _readTimingTracks ? _readTimingTracks() : models::TimingTracksSummary{};
    }

    [[nodiscard]] models::TimingMarksSummary getMarks(const models::TimingMarksRequest& request) const {
        return _readTimingMarks ? _readTimingMarks(request) : models::TimingMarksSummary{};
    }

    [[nodiscard]] models::EnsureTimingTrackResult ensureTrack(const models::EnsureTimingTrackRequest& request) const {
        return _ensureTimingTrack ? _ensureTimingTrack(request) : models::EnsureTimingTrackResult{};
    }

    [[nodiscard]] models::AddTimingMarksResult addMarks(const models::AddTimingMarksRequest& request) const {
        return _addTimingMarks ? _addTimingMarks(request) : models::AddTimingMarksResult{};
    }

private:
    ReadTimingTracksFn _readTimingTracks;
    ReadTimingMarksFn _readTimingMarks;
    EnsureTimingTrackFn _ensureTimingTrack;
    AddTimingMarksFn _addTimingMarks;
};

} // namespace xLightsDesigner::api::services
