#pragma once

#include <functional>
#include <utility>

#include "../models/MediaModels.h"

namespace xLightsDesigner::api::services {

// Media service facade for current audio/show-folder state and sandbox/path
// access validation.
class MediaService {
public:
    using ReadCurrentMediaFn = std::function<models::MediaSummary()>;
    using ReadMediaDirectoriesFn = std::function<models::MediaDirectoriesSummary()>;
    using SetShowDirectoryFn = std::function<models::MediaShowDirectoryResult(const models::MediaShowDirectoryRequest&)>;
    using RequestShowDirectoryAccessFn = std::function<models::MediaShowDirectoryResult(const models::MediaShowDirectoryRequest&)>;
    using ValidatePathAccessFn = std::function<models::MediaPathAccessValidationResult(const models::MediaPathAccessValidationRequest&)>;
    using ReadAudioCapabilitiesFn = std::function<models::MediaAudioCapabilitiesSummary()>;
    using AnalyzeAudioFn = std::function<models::MediaAudioAnalysisSummary()>;

    MediaService(ReadCurrentMediaFn readCurrentMedia,
                 ReadMediaDirectoriesFn readMediaDirectories,
                 SetShowDirectoryFn setShowDirectory,
                 RequestShowDirectoryAccessFn requestShowDirectoryAccess,
                 ValidatePathAccessFn validatePathAccess,
                 ReadAudioCapabilitiesFn readAudioCapabilities,
                 AnalyzeAudioFn analyzeAudio)
        : _readCurrentMedia(std::move(readCurrentMedia)),
          _readMediaDirectories(std::move(readMediaDirectories)),
          _setShowDirectory(std::move(setShowDirectory)),
          _requestShowDirectoryAccess(std::move(requestShowDirectoryAccess)),
          _validatePathAccess(std::move(validatePathAccess)),
          _readAudioCapabilities(std::move(readAudioCapabilities)),
          _analyzeAudio(std::move(analyzeAudio)) {}

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

    [[nodiscard]] models::MediaPathAccessValidationResult validatePathAccess(const models::MediaPathAccessValidationRequest& request) const {
        return _validatePathAccess ? _validatePathAccess(request) : models::MediaPathAccessValidationResult{};
    }

    [[nodiscard]] models::MediaAudioCapabilitiesSummary getAudioCapabilities() const {
        return _readAudioCapabilities ? _readAudioCapabilities() : models::MediaAudioCapabilitiesSummary{};
    }

    [[nodiscard]] models::MediaAudioAnalysisSummary analyzeAudio() const {
        return _analyzeAudio ? _analyzeAudio() : models::MediaAudioAnalysisSummary{};
    }

private:
    ReadCurrentMediaFn _readCurrentMedia;
    ReadMediaDirectoriesFn _readMediaDirectories;
    SetShowDirectoryFn _setShowDirectory;
    RequestShowDirectoryAccessFn _requestShowDirectoryAccess;
    ValidatePathAccessFn _validatePathAccess;
    ReadAudioCapabilitiesFn _readAudioCapabilities;
    AnalyzeAudioFn _analyzeAudio;
};

} // namespace xLightsDesigner::api::services
