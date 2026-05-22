#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "DataLayerModels.h"

namespace xLightsDesigner::api::models {

// Sequence models describe lifecycle, settings, render, preview, and final
// output state without exposing xLights UI classes through the transport layer.
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
    std::optional<bool> supportsModelBlending;
    std::optional<bool> hasUnsavedChanges;
};

struct SequenceSettingsUpdateRequest {
    std::optional<std::string> sequenceType;
    std::optional<int> durationMs;
    std::optional<int> frameMs;
    std::optional<bool> supportsModelBlending;
    std::optional<std::string> metadataAuthor;
    std::optional<std::string> metadataAuthorEmail;
    std::optional<std::string> metadataWebsite;
    std::optional<std::string> metadataSong;
    std::optional<std::string> metadataArtist;
    std::optional<std::string> metadataAlbum;
    std::optional<std::string> metadataMusicUrl;
    std::optional<std::string> metadataComment;
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
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceCloseResult {
    bool closed = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceFocusResult {
    bool focused = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceRenderResult {
    bool rendered = false;
    SequenceSummary sequence;
    std::optional<std::string> fseqPath;
};

struct SequenceCheckIssue {
    std::string type;
    std::string message;
    std::string category;
    std::string modelName;
    std::string effectName;
    int startTimeMs = -1;
    int layerIndex = -1;
};

struct SequenceCheckSection {
    std::string id;
    std::string title;
    std::string description;
    int errorCount = 0;
    int warningCount = 0;
    std::vector<SequenceCheckIssue> issues;
};

struct SequenceCheckResult {
    bool checked = false;
    bool sequenceOpen = false;
    int errorCount = 0;
    int warningCount = 0;
    std::string showDirectory;
    std::string sequencePath;
    std::string generatedAt;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
    std::vector<SequenceCheckSection> sections;
};

struct SequencePreviewVideoExportRequest {
    std::string file;
    bool renderFirst = false;
    int width = 0;
    int height = 0;
};

struct SequencePreviewVideoExportResult {
    bool exported = false;
    std::optional<std::string> file;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSummary sequence;
};

struct SequenceSettingsUpdateResult {
    bool updated = false;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    SequenceSettings settings;
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

struct SequenceFinalFseqState {
    bool sequenceOpen = false;
    std::string sequencePath;
    std::string revisionToken;
    std::string finalFseqPath;
    bool exists = false;
    bool readable = false;
    std::string freshness = "unknown";
    std::optional<FseqFileSummary> fseq;
};

struct SequenceSyncHealthSummary {
    bool sequenceOpen = false;
    std::string status = "unknown";
    std::string sequencePath;
    std::string revisionToken;
    std::vector<std::string> warnings;
    DataLayerListSummary dataLayers;
    SequenceFinalFseqState finalFseq;
};

} // namespace xLightsDesigner::api::models
