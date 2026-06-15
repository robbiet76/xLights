#pragma once

#include <optional>
#include <string>
#include <vector>
#include <map>

namespace xLightsDesigner::api::models {

// Media models cover current show-folder/audio state and sandbox-safe path
// access checks.
struct MediaSummary {
    bool sequenceOpen = false;
    std::optional<std::string> sequencePath;
    std::optional<std::string> mediaFile;
    std::optional<std::string> showDirectory;
    std::optional<int> durationMs;
    std::optional<int> sampleRate;
    std::optional<int> channelCount;
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

struct MediaAudioAnalysisMark {
    int startMs = 0;
    std::optional<int> endMs;
    std::string label;
    std::optional<double> confidence;
    std::string markType;
};

struct MediaAudioAnalysisTrack {
    std::string trackName;
    std::string trackType;
    std::string description;
    std::string timingGranularity;
    std::optional<double> confidence;
    std::vector<MediaAudioAnalysisMark> marks;
};

struct MediaAudioAnalysisEvidence {
    std::string evidenceType;
    std::string description;
    std::optional<double> confidence;
    std::map<std::string, std::string> summary;
};

struct MediaAudioAnalysisSummary {
    bool sequenceOpen = false;
    bool mediaAvailable = false;
    std::optional<std::string> sequencePath;
    std::optional<std::string> mediaFile;
    std::optional<std::string> mediaHash;
    std::optional<int> durationMs;
    std::optional<int> sampleRate;
    std::optional<int> channelCount;
    std::vector<MediaAudioAnalysisTrack> timingTracks;
    std::vector<MediaAudioAnalysisEvidence> evidence;
    std::vector<std::string> warnings;
};

} // namespace xLightsDesigner::api::models
