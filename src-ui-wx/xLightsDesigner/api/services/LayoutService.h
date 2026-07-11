#pragma once

#include <functional>
#include <utility>

#include "../models/LayoutModels.h"

namespace xLightsDesigner::api::services {

// Read-only display/layout service. The designer uses this structure as the
// physical guardrail for native-effect planning, proof validation, and optional
// advanced direct-channel output.
class LayoutService {
public:
    using ReadModelsFn = std::function<models::LayoutModelsSummary()>;
    using ReadSubmodelsFn = std::function<models::LayoutSubmodelsSummary()>;
    using ReadModelNodesFn = std::function<models::LayoutModelNodesSummary(const models::LayoutModelNodesRequest&)>;
    using ReadRenderBufferNodesFn = std::function<models::LayoutRenderBufferNodesSummary(const models::LayoutRenderBufferNodesRequest&)>;
    using ReadChannelMapFn = std::function<models::LayoutChannelMapSummary()>;
    using ReadSettingsFn = std::function<models::LayoutSettingsSummary()>;
    using ReadGroupMembershipsFn = std::function<models::LayoutGroupMembershipsSummary()>;
    using ReadPreviewGroupsFn = std::function<models::LayoutPreviewGroupsSummary()>;
    LayoutService(ReadModelsFn readModels,
                  ReadSubmodelsFn readSubmodels,
                  ReadModelNodesFn readModelNodes,
                  ReadRenderBufferNodesFn readRenderBufferNodes,
                  ReadChannelMapFn readChannelMap,
                  ReadSettingsFn readSettings,
                  ReadGroupMembershipsFn readGroupMemberships,
                  ReadPreviewGroupsFn readPreviewGroups)
        : _readModels(std::move(readModels)),
          _readSubmodels(std::move(readSubmodels)),
          _readModelNodes(std::move(readModelNodes)),
          _readRenderBufferNodes(std::move(readRenderBufferNodes)),
          _readChannelMap(std::move(readChannelMap)),
          _readSettings(std::move(readSettings)),
          _readGroupMemberships(std::move(readGroupMemberships)),
          _readPreviewGroups(std::move(readPreviewGroups)) {}

    [[nodiscard]] models::LayoutModelsSummary getModels() const {
        return _readModels ? _readModels() : models::LayoutModelsSummary{};
    }

    [[nodiscard]] models::LayoutGroupMembershipsSummary getGroupMemberships() const {
        return _readGroupMemberships ? _readGroupMemberships() : models::LayoutGroupMembershipsSummary{};
    }

    [[nodiscard]] models::LayoutPreviewGroupsSummary getPreviewGroups() const {
        return _readPreviewGroups ? _readPreviewGroups() : models::LayoutPreviewGroupsSummary{};
    }

    [[nodiscard]] models::LayoutSubmodelsSummary getSubmodels() const {
        return _readSubmodels ? _readSubmodels() : models::LayoutSubmodelsSummary{};
    }

    [[nodiscard]] models::LayoutModelNodesSummary getModelNodes(const models::LayoutModelNodesRequest& request) const {
        return _readModelNodes ? _readModelNodes(request) : models::LayoutModelNodesSummary{};
    }

    [[nodiscard]] models::LayoutRenderBufferNodesSummary getRenderBufferNodes(const models::LayoutRenderBufferNodesRequest& request) const {
        return _readRenderBufferNodes ? _readRenderBufferNodes(request) : models::LayoutRenderBufferNodesSummary{};
    }

    [[nodiscard]] models::LayoutChannelMapSummary getChannelMap() const {
        return _readChannelMap ? _readChannelMap() : models::LayoutChannelMapSummary{};
    }

    [[nodiscard]] models::LayoutSettingsSummary getSettings() const {
        return _readSettings ? _readSettings() : models::LayoutSettingsSummary{};
    }

private:
    ReadModelsFn _readModels;
    ReadSubmodelsFn _readSubmodels;
    ReadModelNodesFn _readModelNodes;
    ReadRenderBufferNodesFn _readRenderBufferNodes;
    ReadChannelMapFn _readChannelMap;
    ReadSettingsFn _readSettings;
    ReadGroupMembershipsFn _readGroupMemberships;
    ReadPreviewGroupsFn _readPreviewGroups;
};

} // namespace xLightsDesigner::api::services
