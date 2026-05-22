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

struct MediaPathAccessCheckRequest {
    std::string kind;
    std::string path;
    bool mustExist = true;
    bool requireReadable = true;
    bool requireWritable = false;
};

struct MediaPathAccessCheckResult {
    std::string kind;
    std::string path;
    bool exists = false;
    bool readable = false;
    bool writable = false;
    bool accessible = false;
    bool trustedRoot = false;
    bool withinCurrentShowDirectory = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    std::vector<std::string> warnings;
};

struct MediaPathAccessValidationRequest {
    std::vector<MediaPathAccessCheckRequest> checks;
};

struct MediaPathAccessValidationResult {
    bool ok = true;
    std::vector<MediaPathAccessCheckResult> checks;
};

struct MediaAudioCapability {
    std::string capabilityId;
    bool available = false;
    std::string provider;
    std::string reason;
};

struct MediaAudioCapabilitiesSummary {
    bool sequenceOpen = false;
    bool mediaAvailable = false;
    std::optional<std::string> mediaFile;
    std::vector<MediaAudioCapability> capabilities;
    std::vector<std::string> warnings;
};

} // namespace xLightsDesigner::api::models
