#pragma once

#include <string>
#include <vector>

#include <log.h>

#include "DesignerApiHarness.h"

namespace xLightsDesigner {

struct DesignerApiSmokeResult {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

inline DesignerApiSmokeResult RunDesignerApiSmoke() {
    DesignerApiSmokeResult result;
    DesignerApiHarness harness;

    const auto run = [&](const std::string& name, auto fn) {
        auto response = fn();
        if (!response.has_value()) {
            result.failed++;
            result.failures.push_back(name + ": no response");
            spdlog::error("xLightsDesigner smoke failed: {} returned no response.", name);
            return;
        }
        result.passed++;
        spdlog::info("xLightsDesigner smoke {} => {}", name, response->dump());
    };

    run("GET /xlightsdesigner/api/sequence/open", [&]() {
        return harness.invokeEndpoint("GET", "/xlightsdesigner/api/sequence/open");
    });

    run("GET /xlightsdesigner/api/media/current", [&]() {
        return harness.invokeEndpoint("GET", "/xlightsdesigner/api/media/current");
    });

    run("GET /xlightsdesigner/api/timing/tracks", [&]() {
        return harness.invokeEndpoint("GET", "/xlightsdesigner/api/timing/tracks");
    });

    run("GET /xlightsdesigner/api/layout/models", [&]() {
        return harness.invokeEndpoint("GET", "/xlightsdesigner/api/layout/models");
    });

    run("command sequence.getOpen", [&]() {
        return harness.invokeCommand("sequence.getOpen");
    });

    return result;
}

} // namespace xLightsDesigner
