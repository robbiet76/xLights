#pragma once

#include <functional>

#include "../models/ElementModels.h"

namespace xLightsDesigner::api::services {

class ElementService {
public:
    using ReadElementsFn = std::function<models::ElementsSummary()>;

    explicit ElementService(ReadElementsFn readElements)
        : _readElements(std::move(readElements)) {}

    [[nodiscard]] models::ElementsSummary getSummary() const {
        return _readElements ? _readElements() : models::ElementsSummary{};
    }

private:
    ReadElementsFn _readElements;
};

} // namespace xLightsDesigner::api::services
