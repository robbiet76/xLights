#pragma once

#include <string>
#include <vector>

#include <log.h>

#include "DesignerApiHarness.h"

namespace xLightsDesigner {

// Runtime smoke checks intentionally stay small: they verify that the active
// xLights frame can answer the core read routes and one render-sample request.
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

    run("GET /xlightsdesigner/api/layout/scene", [&]() {
        return harness.invokeEndpoint("GET", "/xlightsdesigner/api/layout/scene");
    });

    run("POST /xlightsdesigner/api/sequence/render-samples", [&]() {
        return harness.invokeEndpoint(
            "POST",
            "/xlightsdesigner/api/sequence/render-samples",
            {},
            nlohmann::json{
                {"startMs", 0},
                {"endMs", 25},
                {"maxFrames", 1},
                {"channelRanges", nlohmann::json::array({nlohmann::json{{"startChannel", 1}, {"channelCount", 1}}})}
            }
        );
    });

    run("command sequence.getOpen", [&]() {
        return harness.invokeCommand("sequence.getOpen");
    });

    return result;
}

} // namespace xLightsDesigner
