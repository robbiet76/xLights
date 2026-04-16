#pragma once

#include <functional>

#include "../models/MediaModels.h"

namespace xLightsDesigner::api::services {

class MediaService {
public:
    using ReadCurrentMediaFn = std::function<models::MediaSummary()>;
    using ReadMediaDirectoriesFn = std::function<models::MediaDirectoriesSummary()>;

    MediaService(ReadCurrentMediaFn readCurrentMedia,
                 ReadMediaDirectoriesFn readMediaDirectories)
        : _readCurrentMedia(std::move(readCurrentMedia)),
          _readMediaDirectories(std::move(readMediaDirectories)) {}

    [[nodiscard]] models::MediaSummary getCurrent() const {
        return _readCurrentMedia ? _readCurrentMedia() : models::MediaSummary{};
    }

    [[nodiscard]] models::MediaDirectoriesSummary getDirectories() const {
        return _readMediaDirectories ? _readMediaDirectories() : models::MediaDirectoriesSummary{};
    }

private:
    ReadCurrentMediaFn _readCurrentMedia;
    ReadMediaDirectoriesFn _readMediaDirectories;
};

} // namespace xLightsDesigner::api::services
