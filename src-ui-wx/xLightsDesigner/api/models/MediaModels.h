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

} // namespace xLightsDesigner::api::models
