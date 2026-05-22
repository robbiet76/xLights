#pragma once

#include <functional>
#include <utility>

#include "../models/LayoutModels.h"

namespace xLightsDesigner::api::services {

// Read-only display/layout service. The designer uses this structure as the
// physical guardrail for target-render generation and direct-channel output.
class LayoutService {
public:
    using ReadModelsFn = std::function<models::LayoutModelsSummary()>;
    using ReadSubmodelsFn = std::function<models::LayoutSubmodelsSummary()>;
    using ReadModelNodesFn = std::function<models::LayoutModelNodesSummary(const models::LayoutModelNodesRequest&)>;
    using ReadChannelMapFn = std::function<models::LayoutChannelMapSummary()>;
    using ReadSettingsFn = std::function<models::LayoutSettingsSummary()>;
    using ReadGroupMembershipsFn = std::function<models::LayoutGroupMembershipsSummary()>;
    LayoutService(ReadModelsFn readModels,
                  ReadSubmodelsFn readSubmodels,
                  ReadModelNodesFn readModelNodes,
                  ReadChannelMapFn readChannelMap,
                  ReadSettingsFn readSettings,
                  ReadGroupMembershipsFn readGroupMemberships)
        : _readModels(std::move(readModels)),
          _readSubmodels(std::move(readSubmodels)),
          _readModelNodes(std::move(readModelNodes)),
          _readChannelMap(std::move(readChannelMap)),
          _readSettings(std::move(readSettings)),
          _readGroupMemberships(std::move(readGroupMemberships)) {}

    [[nodiscard]] models::LayoutModelsSummary getModels() const {
        return _readModels ? _readModels() : models::LayoutModelsSummary{};
    }

    [[nodiscard]] models::LayoutGroupMembershipsSummary getGroupMemberships() const {
        return _readGroupMemberships ? _readGroupMemberships() : models::LayoutGroupMembershipsSummary{};
    }

    [[nodiscard]] models::LayoutSubmodelsSummary getSubmodels() const {
        return _readSubmodels ? _readSubmodels() : models::LayoutSubmodelsSummary{};
    }

    [[nodiscard]] models::LayoutModelNodesSummary getModelNodes(const models::LayoutModelNodesRequest& request) const {
        return _readModelNodes ? _readModelNodes(request) : models::LayoutModelNodesSummary{};
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
    ReadChannelMapFn _readChannelMap;
    ReadSettingsFn _readSettings;
    ReadGroupMembershipsFn _readGroupMemberships;
};

} // namespace xLightsDesigner::api::services
