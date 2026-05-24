#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// DataLayer models describe generated XLD FSEQ files, their xLights layer
// placement, and the manifest evidence needed to detect stale output.
struct FseqFileSummary {
    bool exists = false;
    bool readable = false;
    std::string path;
    int versionMajor = 0;
    int versionMinor = 0;
    int frameMs = 0;
    int frameCount = 0;
    int channelCount = 0;
    int maxChannel = 0;
    std::string modifiedAt;
};

struct XldManifestValidationSummary {
    bool expected = false;
    bool exists = false;
    bool readable = false;
    std::string path;
    std::string status = "unknown";
    std::vector<std::string> issueCodes;
    std::vector<std::string> warnings;
    std::string artifactType;
    int artifactVersion = 0;
    std::string appId;
    std::string targetSequencePath;
    std::string generatedFseqPath;
    std::string generatedFseqBasename;
    long long generatedFseqSizeBytes = 0;
    std::string displaySnapshotId;
    std::string channelMapSnapshotId;
    std::string channelMapFingerprint;
    std::string outputConfigurationFingerprint;
    int frameMs = 0;
    int frameCount = 0;
    int channelCount = 0;
    int maxChannel = 0;
    std::string dataLayerName;
    std::string generatedAt;
};

struct DataLayerSummary {
    int index = -1;
    std::string name;
    std::string source;
    std::string dataSource;
    std::string placement = "unknown";
    std::string pathMode = "absolute";
    bool isNutcracker = false;
    bool isXldLayer = false;
    bool pathExists = false;
    bool participatesInFinalRender = false;
    int numChannels = 0;
    int numFrames = 0;
    int channelOffset = 0;
    int lorConvertParams = 0;
    std::optional<FseqFileSummary> fseq;
    std::optional<XldManifestValidationSummary> xldManifest;
};

struct DataLayerListSummary {
    bool sequenceOpen = false;
    std::string sequencePath;
    std::string revisionToken;
    std::vector<DataLayerSummary> layers;
};

struct DataLayerUpsertRequest {
    std::string name;
    std::string sourceFseqPath;
    std::string dataSourcePath;
    std::string placement = "above-nutcracker";
    int channelCount = 0;
    int frameCount = 0;
    int frameMs = 0;
    int channelOffset = 0;
    std::string expectedChecksum;
    std::string pathMode = "relative-preferred";
};

struct DataLayerMutationResult {
    bool ok = false;
    bool sequenceOpen = false;
    std::string sequencePath;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    DataLayerSummary layer;
    DataLayerListSummary layers;
};

struct DataLayerRemoveRequest {
    std::string name;
    int index = -1;
};

struct DataLayerReorderRequest {
    std::string name;
    int index = -1;
    int targetIndex = -1;
    std::string placement;
};

struct DataLayerValidateRequest {
    std::string name;
    int index = -1;
    std::string fseqPath;
    int expectedChannelCount = 0;
    int expectedFrameCount = 0;
    int expectedFrameMs = 0;
};

struct DataLayerValidationResult {
    bool ok = false;
    bool sequenceOpen = false;
    bool fileExists = false;
    bool readable = false;
    bool supportedVersion = false;
    std::string path;
    std::optional<std::string> errorCode;
    std::optional<std::string> errorMessage;
    DataLayerSummary layer;
    FseqFileSummary fseq;
    std::vector<std::string> warnings;
};

} // namespace xLightsDesigner::api::models
