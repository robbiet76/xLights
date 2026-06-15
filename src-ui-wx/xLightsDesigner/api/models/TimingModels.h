#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xLightsDesigner::api::models {

// Timing models represent visible xLights timing tracks and marks. xLights owns
// track creation/editing; XLD reads these tracks as auditable musical anchors.
struct TimingTrackSummary {
    std::string name;
    std::string type;
    std::string subType;
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

struct SongStructureRegionSummary {
    int id = 0;
    int startMs = 0;
    int endMs = 0;
    std::string name;
    std::string colorARGB;
};

struct SongStructureViewSummary {
    int viewIndex = -1;
    std::string name;
    bool active = false;
    std::vector<SongStructureRegionSummary> regions;
};

struct SongStructureSummary {
    bool sequenceOpen = false;
    bool hasRegions = false;
    int activeViewIndex = -1;
    std::string activeViewName;
    std::string revisionToken;
    std::vector<SongStructureViewSummary> views;
};

} // namespace xLightsDesigner::api::models
