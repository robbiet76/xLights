#pragma once

#include <functional>
#include <utility>

#include "../models/DataLayerModels.h"

namespace xLightsDesigner::api::services {

class DataLayerService {
public:
    using ReadDataLayersFn = std::function<models::DataLayerListSummary()>;
    using UpsertDataLayerFn = std::function<models::DataLayerMutationResult(const models::DataLayerUpsertRequest&)>;
    using RemoveDataLayerFn = std::function<models::DataLayerMutationResult(const models::DataLayerRemoveRequest&)>;
    using ReorderDataLayerFn = std::function<models::DataLayerMutationResult(const models::DataLayerReorderRequest&)>;
    using ValidateDataLayerFn = std::function<models::DataLayerValidationResult(const models::DataLayerValidateRequest&)>;

    DataLayerService(ReadDataLayersFn readDataLayers,
                     UpsertDataLayerFn upsertDataLayer,
                     RemoveDataLayerFn removeDataLayer,
                     ReorderDataLayerFn reorderDataLayer,
                     ValidateDataLayerFn validateDataLayer)
        : _readDataLayers(std::move(readDataLayers)),
          _upsertDataLayer(std::move(upsertDataLayer)),
          _removeDataLayer(std::move(removeDataLayer)),
          _reorderDataLayer(std::move(reorderDataLayer)),
          _validateDataLayer(std::move(validateDataLayer)) {}

    [[nodiscard]] models::DataLayerListSummary getDataLayers() const {
        return _readDataLayers ? _readDataLayers() : models::DataLayerListSummary{};
    }

    [[nodiscard]] models::DataLayerMutationResult upsertDataLayer(const models::DataLayerUpsertRequest& request) const {
        return _upsertDataLayer ? _upsertDataLayer(request) : models::DataLayerMutationResult{};
    }

    [[nodiscard]] models::DataLayerMutationResult removeDataLayer(const models::DataLayerRemoveRequest& request) const {
        return _removeDataLayer ? _removeDataLayer(request) : models::DataLayerMutationResult{};
    }

    [[nodiscard]] models::DataLayerMutationResult reorderDataLayer(const models::DataLayerReorderRequest& request) const {
        return _reorderDataLayer ? _reorderDataLayer(request) : models::DataLayerMutationResult{};
    }

    [[nodiscard]] models::DataLayerValidationResult validateDataLayer(const models::DataLayerValidateRequest& request) const {
        return _validateDataLayer ? _validateDataLayer(request) : models::DataLayerValidationResult{};
    }

private:
    ReadDataLayersFn _readDataLayers;
    UpsertDataLayerFn _upsertDataLayer;
    RemoveDataLayerFn _removeDataLayer;
    ReorderDataLayerFn _reorderDataLayer;
    ValidateDataLayerFn _validateDataLayer;
};

} // namespace xLightsDesigner::api::services
