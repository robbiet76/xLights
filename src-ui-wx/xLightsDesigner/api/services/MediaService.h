#pragma once

#include <functional>

#include "../models/MediaModels.h"

namespace xLightsDesigner::api::services {

class MediaService {
public:
    using ReadCurrentMediaFn = std::function<models::MediaSummary()>;
    using ReadMediaDirectoriesFn = std::function<models::MediaDirectoriesSummary()>;
    using SetShowDirectoryFn = std::function<models::MediaShowDirectoryResult(const models::MediaShowDirectoryRequest&)>;
    using RequestShowDirectoryAccessFn = std::function<models::MediaShowDirectoryResult(const models::MediaShowDirectoryRequest&)>;

    MediaService(ReadCurrentMediaFn readCurrentMedia,
                 ReadMediaDirectoriesFn readMediaDirectories,
                 SetShowDirectoryFn setShowDirectory,
                 RequestShowDirectoryAccessFn requestShowDirectoryAccess)
        : _readCurrentMedia(std::move(readCurrentMedia)),
          _readMediaDirectories(std::move(readMediaDirectories)),
          _setShowDirectory(std::move(setShowDirectory)),
          _requestShowDirectoryAccess(std::move(requestShowDirectoryAccess)) {}

    [[nodiscard]] models::MediaSummary getCurrent() const {
        return _readCurrentMedia ? _readCurrentMedia() : models::MediaSummary{};
    }

    [[nodiscard]] models::MediaDirectoriesSummary getDirectories() const {
        return _readMediaDirectories ? _readMediaDirectories() : models::MediaDirectoriesSummary{};
    }

    [[nodiscard]] models::MediaShowDirectoryResult setShowDirectory(const models::MediaShowDirectoryRequest& request) const {
        return _setShowDirectory ? _setShowDirectory(request) : models::MediaShowDirectoryResult{};
    }

    [[nodiscard]] models::MediaShowDirectoryResult requestShowDirectoryAccess(const models::MediaShowDirectoryRequest& request) const {
        return _requestShowDirectoryAccess ? _requestShowDirectoryAccess(request) : models::MediaShowDirectoryResult{};
    }

private:
    ReadCurrentMediaFn _readCurrentMedia;
    ReadMediaDirectoriesFn _readMediaDirectories;
    SetShowDirectoryFn _setShowDirectory;
    RequestShowDirectoryAccessFn _requestShowDirectoryAccess;
};

} // namespace xLightsDesigner::api::services
