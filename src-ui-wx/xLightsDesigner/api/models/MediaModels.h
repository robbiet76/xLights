#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

struct MediaSummary {
    bool sequenceOpen = false;
    std::optional<std::string> sequencePath;
    std::optional<std::string> mediaFile;
    std::optional<std::string> showDirectory;
};

struct MediaDirectoriesSummary {
    std::vector<std::string> directories;
};

struct MediaShowDirectoryRequest {
    std::string showDirectory;
    bool force = false;
    bool permanent = false;
};

struct MediaShowDirectoryResult {
    bool changed = false;
    bool sequenceClosed = false;
    std::optional<std::string> previousShowDirectory;
    std::optional<std::string> showDirectory;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
};

} // namespace xLightsDesigner::api::models
