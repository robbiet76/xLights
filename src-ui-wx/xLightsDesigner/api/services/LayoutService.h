#pragma once

#include <functional>

#include "../models/LayoutModels.h"

namespace xLightsDesigner::api::services {

class LayoutService {
public:
    using ReadModelsFn = std::function<models::LayoutModelsSummary()>;
    using ReadSubmodelsFn = std::function<models::LayoutSubmodelsSummary()>;
    using ReadModelNodesFn = std::function<models::LayoutModelNodesSummary(const models::LayoutModelNodesRequest&)>;
    using ReadChannelMapFn = std::function<models::LayoutChannelMapSummary()>;
    using ReadSettingsFn = std::function<models::LayoutSettingsSummary()>;
    using ReadGroupMembershipsFn = std::function<models::LayoutGroupMembershipsSummary()>;
    using CreateCustomModelFn = std::function<models::CreateCustomModelResult(const models::CreateCustomModelRequest&)>;

    LayoutService(ReadModelsFn readModels,
                  ReadSubmodelsFn readSubmodels,
                  ReadModelNodesFn readModelNodes,
                  ReadChannelMapFn readChannelMap,
                  ReadSettingsFn readSettings,
                  ReadGroupMembershipsFn readGroupMemberships,
                  CreateCustomModelFn createCustomModel)
        : _readModels(std::move(readModels)),
          _readSubmodels(std::move(readSubmodels)),
          _readModelNodes(std::move(readModelNodes)),
          _readChannelMap(std::move(readChannelMap)),
          _readSettings(std::move(readSettings)),
          _readGroupMemberships(std::move(readGroupMemberships)),
          _createCustomModel(std::move(createCustomModel)) {}

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

    [[nodiscard]] models::CreateCustomModelResult createCustomModel(const models::CreateCustomModelRequest& request) const {
        return _createCustomModel ? _createCustomModel(request) : models::CreateCustomModelResult{};
    }

private:
    ReadModelsFn _readModels;
    ReadSubmodelsFn _readSubmodels;
    ReadModelNodesFn _readModelNodes;
    ReadChannelMapFn _readChannelMap;
    ReadSettingsFn _readSettings;
    ReadGroupMembershipsFn _readGroupMemberships;
    CreateCustomModelFn _createCustomModel;
};

} // namespace xLightsDesigner::api::services
