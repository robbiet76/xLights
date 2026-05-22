#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

struct TimingTrackSummary {
    std::string name;
    std::string type;
    int markCount = 0;
    int layerCount = 0;
    std::string revisionToken;
};

struct TimingTracksSummary {
    bool sequenceOpen = false;
    std::vector<TimingTrackSummary> tracks;
};

struct TimingMarksRequest {
    std::string trackName;
    std::optional<int> startMs;
    std::optional<int> endMs;
};

struct TimingMarkSummary {
    int startMs = 0;
    int endMs = 0;
    std::string label;
    int layerNumber = 0;
};

struct TimingMarksSummary {
    bool sequenceOpen = false;
    bool trackFound = false;
    std::string trackName;
    std::string revisionToken;
    std::vector<TimingMarkSummary> marks;
};

struct EnsureTimingTrackRequest {
    std::string trackName;
    std::string subType;
};

struct EnsureTimingTrackResult {
    bool sequenceOpen = false;
    bool created = false;
    std::string requestedTrackName;
    std::string actualTrackName;
    std::string subType;
};

struct TimingMarkInput {
    int startMs = 0;
    int endMs = 0;
    std::string label;
};

struct AddTimingMarksRequest {
    std::string trackName;
    std::string subType;
    bool replaceExisting = false;
    std::vector<TimingMarkInput> marks;
};

struct AddTimingMarksResult {
    bool sequenceOpen = false;
    bool trackCreated = false;
    bool trackFound = false;
    std::string requestedTrackName;
    std::string actualTrackName;
    int addedMarkCount = 0;
};

} // namespace xLightsDesigner::api::models
