#pragma once

#include <functional>

#include "../models/EffectModels.h"

namespace xLightsDesigner::api::services {

class EffectService {
public:
    using ReadEffectWindowFn = std::function<models::EffectWindowSummary(const models::EffectWindowRequest&)>;
    using AddEffectFn = std::function<models::AddEffectResult(const models::AddEffectRequest&)>;
    using ClearEffectWindowFn = std::function<models::ClearEffectWindowResult(const models::ClearEffectWindowRequest&)>;
    using ApplyEffectBatchFn = std::function<models::ApplyEffectBatchResult(const models::ApplyEffectBatchRequest&)>;

    EffectService(ReadEffectWindowFn readEffectWindow,
                  AddEffectFn addEffect,
                  ClearEffectWindowFn clearEffectWindow,
                  ApplyEffectBatchFn applyEffectBatch)
        : _readEffectWindow(std::move(readEffectWindow)),
          _addEffect(std::move(addEffect)),
          _clearEffectWindow(std::move(clearEffectWindow)),
          _applyEffectBatch(std::move(applyEffectBatch)) {}

    [[nodiscard]] models::EffectWindowSummary getWindow(const models::EffectWindowRequest& request) const {
        return _readEffectWindow ? _readEffectWindow(request) : models::EffectWindowSummary{};
    }

    [[nodiscard]] models::AddEffectResult addEffect(const models::AddEffectRequest& request) const {
        return _addEffect ? _addEffect(request) : models::AddEffectResult{};
    }

    [[nodiscard]] models::ClearEffectWindowResult clearWindow(const models::ClearEffectWindowRequest& request) const {
        return _clearEffectWindow ? _clearEffectWindow(request) : models::ClearEffectWindowResult{};
    }

    [[nodiscard]] models::ApplyEffectBatchResult applyBatch(const models::ApplyEffectBatchRequest& request) const {
        return _applyEffectBatch ? _applyEffectBatch(request) : models::ApplyEffectBatchResult{};
    }

private:
    ReadEffectWindowFn _readEffectWindow;
    AddEffectFn _addEffect;
    ClearEffectWindowFn _clearEffectWindow;
    ApplyEffectBatchFn _applyEffectBatch;
};

} // namespace xLightsDesigner::api::services
