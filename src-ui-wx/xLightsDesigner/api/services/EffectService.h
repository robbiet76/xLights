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
    using CloneEffectsFn = std::function<models::CloneEffectsResult(const models::CloneEffectsRequest&)>;
    using UpdateEffectFn = std::function<models::UpdateEffectResult(const models::UpdateEffectRequest&)>;
    using DeleteEffectsFn = std::function<models::DeleteEffectsResult(const models::DeleteEffectsRequest&)>;
    using DeleteEffectLayerFn = std::function<models::DeleteEffectLayerResult(const models::DeleteEffectLayerRequest&)>;
    using ReorderEffectLayerFn = std::function<models::ReorderEffectLayerResult(const models::ReorderEffectLayerRequest&)>;
    using CompactEffectLayersFn = std::function<models::CompactEffectLayersResult(const models::CompactEffectLayersRequest&)>;

    EffectService(ReadEffectWindowFn readEffectWindow,
                  AddEffectFn addEffect,
                  ClearEffectWindowFn clearEffectWindow,
                  ApplyEffectBatchFn applyEffectBatch,
                  CloneEffectsFn cloneEffects,
                  UpdateEffectFn updateEffect,
                  DeleteEffectsFn deleteEffects,
                  DeleteEffectLayerFn deleteEffectLayer,
                  ReorderEffectLayerFn reorderEffectLayer,
                  CompactEffectLayersFn compactEffectLayers)
        : _readEffectWindow(std::move(readEffectWindow)),
          _addEffect(std::move(addEffect)),
          _clearEffectWindow(std::move(clearEffectWindow)),
          _applyEffectBatch(std::move(applyEffectBatch)),
          _cloneEffects(std::move(cloneEffects)),
          _updateEffect(std::move(updateEffect)),
          _deleteEffects(std::move(deleteEffects)),
          _deleteEffectLayer(std::move(deleteEffectLayer)),
          _reorderEffectLayer(std::move(reorderEffectLayer)),
          _compactEffectLayers(std::move(compactEffectLayers)) {}

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

    [[nodiscard]] models::CloneEffectsResult cloneEffects(const models::CloneEffectsRequest& request) const {
        return _cloneEffects ? _cloneEffects(request) : models::CloneEffectsResult{};
    }

    [[nodiscard]] models::UpdateEffectResult updateEffect(const models::UpdateEffectRequest& request) const {
        return _updateEffect ? _updateEffect(request) : models::UpdateEffectResult{};
    }

    [[nodiscard]] models::DeleteEffectsResult deleteEffects(const models::DeleteEffectsRequest& request) const {
        return _deleteEffects ? _deleteEffects(request) : models::DeleteEffectsResult{};
    }

    [[nodiscard]] models::DeleteEffectLayerResult deleteLayer(const models::DeleteEffectLayerRequest& request) const {
        return _deleteEffectLayer ? _deleteEffectLayer(request) : models::DeleteEffectLayerResult{};
    }

    [[nodiscard]] models::ReorderEffectLayerResult reorderLayer(const models::ReorderEffectLayerRequest& request) const {
        return _reorderEffectLayer ? _reorderEffectLayer(request) : models::ReorderEffectLayerResult{};
    }

    [[nodiscard]] models::CompactEffectLayersResult compactLayers(const models::CompactEffectLayersRequest& request) const {
        return _compactEffectLayers ? _compactEffectLayers(request) : models::CompactEffectLayersResult{};
    }

private:
    ReadEffectWindowFn _readEffectWindow;
    AddEffectFn _addEffect;
    ClearEffectWindowFn _clearEffectWindow;
    ApplyEffectBatchFn _applyEffectBatch;
    CloneEffectsFn _cloneEffects;
    UpdateEffectFn _updateEffect;
    DeleteEffectsFn _deleteEffects;
    DeleteEffectLayerFn _deleteEffectLayer;
    ReorderEffectLayerFn _reorderEffectLayer;
    CompactEffectLayersFn _compactEffectLayers;
};

} // namespace xLightsDesigner::api::services
