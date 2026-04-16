#pragma once

#include <cstdint>

#include <optional>
#include <string>

namespace xLightsDesigner::api::models {

struct SequenceSummary {
    bool isOpen = false;
    std::optional<std::string> path;
    std::optional<std::string> revisionToken;
};

struct SequenceSettings {
    bool isOpen = false;
    std::optional<std::string> path;
    std::optional<std::string> sequenceType;
    std::optional<std::string> mediaFile;
    std::optional<int> durationMs;
    std::optional<int> frameMs;
    std::optional<bool> hasUnsavedChanges;
};

struct SequenceOpenRequest {
    std::string file;
    bool force = false;
};

struct SequenceCreateRequest {
    std::string file;
    std::string mediaFile;
    std::string view;
    int durationMs = 0;
    int frameMs = 25;
    bool overwrite = false;
};

struct SequenceOpenResult {
    bool opened = false;
    bool queued = false;
    std::optional<std::string> requestedPath;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    std::optional<std::uint64_t> retryAfterMs;
    SequenceSummary sequence;
};

struct SequenceCreateResult {
    bool created = false;
    std::optional<std::string> requestedPath;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceSaveResult {
    bool saved = false;
    SequenceSummary sequence;
};

struct SequenceCloseResult {
    bool closed = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceRenderResult {
    bool rendered = false;
    SequenceSummary sequence;
};

struct SequenceChannelRangeRequest {
    int startChannel = 0; // 1-based
    int channelCount = 0;
};

struct SequenceRenderedFrameSample {
    int frameIndex = 0;
    int frameTimeMs = 0;
    std::string dataBase64;
};

struct SequenceRenderSamplesRequest {
    int startMs = 0;
    int endMs = 0;
    int maxFrames = 5;
    int frameStride = 0;
    std::vector<SequenceChannelRangeRequest> channelRanges;
};

struct SequenceRenderSamplesResult {
    bool sequenceOpen = false;
    bool samplesAvailable = false;
    SequenceSummary sequence;
    std::optional<std::string> fseqPath;
    std::optional<int> frameMs;
    std::optional<int> totalFrames;
    std::optional<int> totalChannels;
    int startMs = 0;
    int endMs = 0;
    int sampledFrameCount = 0;
    int sampledChannelCount = 0;
    std::string sampleEncoding = "base64_packed_channel_ranges_v1";
    std::vector<SequenceChannelRangeRequest> channelRanges;
    std::vector<SequenceRenderedFrameSample> samples;
};

} // namespace xLightsDesigner::api::models
