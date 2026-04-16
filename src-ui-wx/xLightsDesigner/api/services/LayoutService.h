#pragma once

#include <functional>

#include "../models/LayoutModels.h"

namespace xLightsDesigner::api::services {

class LayoutService {
public:
    using ReadModelsFn = std::function<models::LayoutModelsSummary()>;
    using ReadSettingsFn = std::function<models::LayoutSettingsSummary()>;
    using ReadGroupMembershipsFn = std::function<models::LayoutGroupMembershipsSummary()>;

    LayoutService(ReadModelsFn readModels, ReadSettingsFn readSettings, ReadGroupMembershipsFn readGroupMemberships)
        : _readModels(std::move(readModels)),
          _readSettings(std::move(readSettings)),
          _readGroupMemberships(std::move(readGroupMemberships)) {}

    [[nodiscard]] models::LayoutModelsSummary getModels() const {
        return _readModels ? _readModels() : models::LayoutModelsSummary{};
    }

    [[nodiscard]] models::LayoutGroupMembershipsSummary getGroupMemberships() const {
        return _readGroupMemberships ? _readGroupMemberships() : models::LayoutGroupMembershipsSummary{};
    }

    [[nodiscard]] models::LayoutSettingsSummary getSettings() const {
        return _readSettings ? _readSettings() : models::LayoutSettingsSummary{};
    }

private:
    ReadModelsFn _readModels;
    ReadSettingsFn _readSettings;
    ReadGroupMembershipsFn _readGroupMemberships;
};

} // namespace xLightsDesigner::api::services
