#pragma once

#include <functional>
#include <utility>

#include "../models/ElementModels.h"

namespace xLightsDesigner::api::services {

// Element service facade for sequence rows and display ordering. This exposes
// ordering state without opening native xLights effect-block mutation.
class ElementService {
public:
    using ReadElementsFn = std::function<models::ElementsSummary()>;
    using ReadDisplayOrderFn = std::function<models::DisplayElementOrderSummary()>;
    using SetDisplayOrderFn = std::function<models::SetDisplayElementOrderResult(const models::SetDisplayElementOrderRequest&)>;

    ElementService(ReadElementsFn readElements,
                   ReadDisplayOrderFn readDisplayOrder,
                   SetDisplayOrderFn setDisplayOrder)
        : _readElements(std::move(readElements)),
          _readDisplayOrder(std::move(readDisplayOrder)),
          _setDisplayOrder(std::move(setDisplayOrder)) {}

    [[nodiscard]] models::ElementsSummary getSummary() const {
        return _readElements ? _readElements() : models::ElementsSummary{};
    }

    [[nodiscard]] models::DisplayElementOrderSummary getDisplayOrder() const {
        return _readDisplayOrder ? _readDisplayOrder() : models::DisplayElementOrderSummary{};
    }

    [[nodiscard]] models::SetDisplayElementOrderResult setDisplayOrder(const models::SetDisplayElementOrderRequest& request) const {
        return _setDisplayOrder ? _setDisplayOrder(request) : models::SetDisplayElementOrderResult{};
    }

private:
    ReadElementsFn _readElements;
    ReadDisplayOrderFn _readDisplayOrder;
    SetDisplayOrderFn _setDisplayOrder;
};

} // namespace xLightsDesigner::api::services
