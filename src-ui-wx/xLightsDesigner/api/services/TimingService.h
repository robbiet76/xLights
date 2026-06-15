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
    using ReadSongStructureFn = std::function<models::SongStructureSummary()>;

    TimingService(ReadTimingTracksFn readTimingTracks,
                  ReadTimingMarksFn readTimingMarks,
                  ReadSongStructureFn readSongStructure)
        : _readTimingTracks(std::move(readTimingTracks)),
          _readTimingMarks(std::move(readTimingMarks)),
          _readSongStructure(std::move(readSongStructure)) {}

    [[nodiscard]] models::TimingTracksSummary getTracks() const {
        return _readTimingTracks ? _readTimingTracks() : models::TimingTracksSummary{};
    }

    [[nodiscard]] models::TimingMarksSummary getMarks(const models::TimingMarksRequest& request) const {
        return _readTimingMarks ? _readTimingMarks(request) : models::TimingMarksSummary{};
    }

    [[nodiscard]] models::SongStructureSummary getSongStructure() const {
        return _readSongStructure ? _readSongStructure() : models::SongStructureSummary{};
    }

private:
    ReadTimingTracksFn _readTimingTracks;
    ReadTimingMarksFn _readTimingMarks;
    ReadSongStructureFn _readSongStructure;
};

} // namespace xLightsDesigner::api::services
