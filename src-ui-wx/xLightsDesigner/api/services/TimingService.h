#pragma once

#include <functional>
#include <utility>

#include "../models/TimingModels.h"

namespace xLightsDesigner::api::services {

// Read-only timing service facade for tracks visible in xLights. Marks are used
// as auditable musical structure, lyric, and custom cue references.
class TimingService {
public:
    using ReadTimingTracksFn = std::function<models::TimingTracksSummary()>;
    using ReadTimingMarksFn = std::function<models::TimingMarksSummary(const models::TimingMarksRequest&)>;

    TimingService(ReadTimingTracksFn readTimingTracks,
                  ReadTimingMarksFn readTimingMarks)
        : _readTimingTracks(std::move(readTimingTracks)),
          _readTimingMarks(std::move(readTimingMarks)) {}

    [[nodiscard]] models::TimingTracksSummary getTracks() const {
        return _readTimingTracks ? _readTimingTracks() : models::TimingTracksSummary{};
    }

    [[nodiscard]] models::TimingMarksSummary getMarks(const models::TimingMarksRequest& request) const {
        return _readTimingMarks ? _readTimingMarks(request) : models::TimingMarksSummary{};
    }

private:
    ReadTimingTracksFn _readTimingTracks;
    ReadTimingMarksFn _readTimingMarks;
};

} // namespace xLightsDesigner::api::services
