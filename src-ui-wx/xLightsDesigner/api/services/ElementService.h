#pragma once

#include <functional>
#include <utility>

#include "../models/ElementModels.h"

namespace xLightsDesigner::api::services {

// Element service facade for sequence rows, display ordering, and sequencer
// selection state.
class ElementService {
public:
    using ReadElementsFn = std::function<models::ElementsSummary()>;
    using ReadDisplayOrderFn = std::function<models::DisplayElementOrderSummary()>;
    using SetDisplayOrderFn = std::function<models::SetDisplayElementOrderResult(const models::SetDisplayElementOrderRequest&)>;
    using EnsureSequenceElementsFn = std::function<models::EnsureSequenceElementsResult(const models::EnsureSequenceElementsRequest&)>;
    using ReadSelectedDisplayElementsFn = std::function<models::SelectedDisplayElementsSummary()>;
    using SetSelectedDisplayElementsFn = std::function<models::SetSelectedDisplayElementsResult(const models::SetSelectedDisplayElementsRequest&)>;
    using SetDisplayElementVisibilityFn = std::function<models::SetDisplayElementVisibilityResult(const models::SetDisplayElementVisibilityRequest&)>;

    ElementService(ReadElementsFn readElements,
                   ReadDisplayOrderFn readDisplayOrder,
                   SetDisplayOrderFn setDisplayOrder,
                   EnsureSequenceElementsFn ensureSequenceElements,
                   ReadSelectedDisplayElementsFn readSelectedDisplayElements,
                   SetSelectedDisplayElementsFn setSelectedDisplayElements,
                   SetDisplayElementVisibilityFn setDisplayElementVisibility)
        : _readElements(std::move(readElements)),
          _readDisplayOrder(std::move(readDisplayOrder)),
          _setDisplayOrder(std::move(setDisplayOrder)),
          _ensureSequenceElements(std::move(ensureSequenceElements)),
          _readSelectedDisplayElements(std::move(readSelectedDisplayElements)),
          _setSelectedDisplayElements(std::move(setSelectedDisplayElements)),
          _setDisplayElementVisibility(std::move(setDisplayElementVisibility)) {}

    [[nodiscard]] models::ElementsSummary getSummary() const {
        return _readElements ? _readElements() : models::ElementsSummary{};
    }

    [[nodiscard]] models::DisplayElementOrderSummary getDisplayOrder() const {
        return _readDisplayOrder ? _readDisplayOrder() : models::DisplayElementOrderSummary{};
    }

    [[nodiscard]] models::SetDisplayElementOrderResult setDisplayOrder(const models::SetDisplayElementOrderRequest& request) const {
        return _setDisplayOrder ? _setDisplayOrder(request) : models::SetDisplayElementOrderResult{};
    }

    [[nodiscard]] models::EnsureSequenceElementsResult ensureSequenceElements(const models::EnsureSequenceElementsRequest& request) const {
        return _ensureSequenceElements ? _ensureSequenceElements(request) : models::EnsureSequenceElementsResult{};
    }

    [[nodiscard]] models::SelectedDisplayElementsSummary getSelectedDisplayElements() const {
        return _readSelectedDisplayElements ? _readSelectedDisplayElements() : models::SelectedDisplayElementsSummary{};
    }

    [[nodiscard]] models::SetSelectedDisplayElementsResult setSelectedDisplayElements(const models::SetSelectedDisplayElementsRequest& request) const {
        return _setSelectedDisplayElements ? _setSelectedDisplayElements(request) : models::SetSelectedDisplayElementsResult{};
    }

    [[nodiscard]] models::SetDisplayElementVisibilityResult setDisplayElementVisibility(const models::SetDisplayElementVisibilityRequest& request) const {
        return _setDisplayElementVisibility ? _setDisplayElementVisibility(request) : models::SetDisplayElementVisibilityResult{};
    }

private:
    ReadElementsFn _readElements;
    ReadDisplayOrderFn _readDisplayOrder;
    SetDisplayOrderFn _setDisplayOrder;
    EnsureSequenceElementsFn _ensureSequenceElements;
    ReadSelectedDisplayElementsFn _readSelectedDisplayElements;
    SetSelectedDisplayElementsFn _setSelectedDisplayElements;
    SetDisplayElementVisibilityFn _setDisplayElementVisibility;
};

} // namespace xLightsDesigner::api::services
