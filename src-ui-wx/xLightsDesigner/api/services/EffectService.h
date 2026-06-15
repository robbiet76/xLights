#pragma once

#include <functional>
#include <utility>

#include "../models/EffectModels.h"

namespace xLightsDesigner::api::services {

// Native effect service facade. The host owns all xLights object access; this
// layer only keeps handler code independent from xLights UI classes.
class EffectService {
public:
    using ListSchemasFn = std::function<models::NativeEffectSchemaResult(const models::NativeEffectSchemaRequest&)>;
    using ListEffectsFn = std::function<models::NativeEffectListResult(const models::NativeEffectListRequest&)>;
    using UpsertEffectFn = std::function<models::NativeEffectMutationResult(const models::NativeEffectUpsertRequest&)>;
    using RemoveEffectFn = std::function<models::NativeEffectMutationResult(const models::NativeEffectRemoveRequest&)>;
    using ListLayersFn = std::function<models::NativeEffectLayerListResult(const models::NativeEffectLayerListRequest&)>;
    using EnsureLayerFn = std::function<models::NativeEffectLayerMutationResult(const models::NativeEffectLayerEnsureRequest&)>;
    using RemoveLayerFn = std::function<models::NativeEffectLayerMutationResult(const models::NativeEffectLayerRemoveRequest&)>;

    EffectService(ListSchemasFn listSchemas,
                  ListEffectsFn listEffects,
                  UpsertEffectFn upsertEffect,
                  RemoveEffectFn removeEffect,
                  ListLayersFn listLayers,
                  EnsureLayerFn ensureLayer,
                  RemoveLayerFn removeLayer)
        : _listSchemas(std::move(listSchemas)),
          _listEffects(std::move(listEffects)),
          _upsertEffect(std::move(upsertEffect)),
          _removeEffect(std::move(removeEffect)),
          _listLayers(std::move(listLayers)),
          _ensureLayer(std::move(ensureLayer)),
          _removeLayer(std::move(removeLayer)) {}

    [[nodiscard]] models::NativeEffectSchemaResult listSchemas(const models::NativeEffectSchemaRequest& request) const {
        return _listSchemas ? _listSchemas(request) : models::NativeEffectSchemaResult{};
    }

    [[nodiscard]] models::NativeEffectListResult listEffects(const models::NativeEffectListRequest& request) const {
        return _listEffects ? _listEffects(request) : models::NativeEffectListResult{};
    }

    [[nodiscard]] models::NativeEffectMutationResult upsertEffect(const models::NativeEffectUpsertRequest& request) const {
        return _upsertEffect ? _upsertEffect(request) : models::NativeEffectMutationResult{};
    }

    [[nodiscard]] models::NativeEffectMutationResult removeEffect(const models::NativeEffectRemoveRequest& request) const {
        return _removeEffect ? _removeEffect(request) : models::NativeEffectMutationResult{};
    }

    [[nodiscard]] models::NativeEffectLayerListResult listLayers(const models::NativeEffectLayerListRequest& request) const {
        return _listLayers ? _listLayers(request) : models::NativeEffectLayerListResult{};
    }

    [[nodiscard]] models::NativeEffectLayerMutationResult ensureLayer(const models::NativeEffectLayerEnsureRequest& request) const {
        return _ensureLayer ? _ensureLayer(request) : models::NativeEffectLayerMutationResult{};
    }

    [[nodiscard]] models::NativeEffectLayerMutationResult removeLayer(const models::NativeEffectLayerRemoveRequest& request) const {
        return _removeLayer ? _removeLayer(request) : models::NativeEffectLayerMutationResult{};
    }

private:
    ListSchemasFn _listSchemas;
    ListEffectsFn _listEffects;
    UpsertEffectFn _upsertEffect;
    RemoveEffectFn _removeEffect;
    ListLayersFn _listLayers;
    EnsureLayerFn _ensureLayer;
    RemoveLayerFn _removeLayer;
};

} // namespace xLightsDesigner::api::services
