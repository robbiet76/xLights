#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "api/transport/ApiResponse.h"
#include "api/transport/EndpointRouter.h"
#include "api/transport/JsonTransport.h"

namespace xLightsDesigner {

struct DesignerApiSelfTestResult {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;

    [[nodiscard]] bool ok() const {
        return failed == 0;
    }

    void pass() {
        passed++;
    }

    void fail(std::string message) {
        failed++;
        failures.push_back(std::move(message));
    }
};

inline void Check(bool condition, const std::string& message, DesignerApiSelfTestResult& result) {
    if (condition) {
        result.pass();
    } else {
        result.fail(message);
    }
}

inline DesignerApiSelfTestResult RunDesignerApiSelfTests() {
    DesignerApiSelfTestResult result;

    {
        api::transport::EndpointRouter router;
        Check(router.resolve("GET", "/xlightsdesigner/api/sequence/open") == std::optional<std::string>("sequence.getOpen"),
              "EndpointRouter should map sequence open endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/metadata/effects/status") == std::optional<std::string>("metadata.effects.status"),
              "EndpointRouter should map metadata effects status endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/scene") == std::optional<std::string>("layout.getScene"),
              "EndpointRouter should map layout scene endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/timing/add-marks") == std::optional<std::string>("timing.addMarks"),
              "EndpointRouter should map timing add-marks endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/render-samples") == std::optional<std::string>("sequence.getRenderSamples"),
              "EndpointRouter should map render samples endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequencing/apply-window-plan") == std::optional<std::string>("sequencing.applyWindowPlan"),
              "EndpointRouter should map sequencing apply-window-plan endpoint.", result);
        Check(!router.resolve("DELETE", "/xlightsdesigner/api/sequence/open").has_value(),
              "EndpointRouter should reject unsupported method/path pairs.", result);
    }

    {
        std::map<std::string, std::string> queryParams = {{"track", "XD: Song Structure"}};
        nlohmann::json body = {
            {"replaceExisting", true},
            {"startMs", 44000},
            {"marks", nlohmann::json::array({
                {
                    {"startMs", 44000},
                    {"endMs", 62000},
                    {"label", "Chorus 1"}
                }
            })}
        };
        auto merged = api::transport::MergeRequestParams(queryParams, body);
        Check(merged["track"] == "XD: Song Structure",
              "MergeRequestParams should preserve query params.", result);
        Check(merged["replaceExisting"] == "true",
              "MergeRequestParams should normalize booleans.", result);
        Check(merged["startMs"] == "44000",
              "MergeRequestParams should normalize integer bodies.", result);
        Check(!merged["marks"].empty() && merged["marks"].front() == '[',
              "MergeRequestParams should serialize structured JSON values.", result);
    }

    {
        api::transport::ApiResponse response;
        response.statusCode = 200;
        response.command = "timing.getTracks";
        response.requestId = "req-1";
        response.data = { {"tracks", nlohmann::json::array()} };

        auto envelope = api::transport::BuildJsonHttpEnvelope(response);
        Check(envelope.value("ok", false) == true,
              "BuildJsonHttpEnvelope should expose ok=true for successful responses.", result);
        Check(envelope.value("command", "") == "timing.getTracks",
              "BuildJsonHttpEnvelope should retain command name.", result);
        Check(envelope.contains("data"),
              "BuildJsonHttpEnvelope should retain response data.", result);
    }

    {
        api::transport::ApiResponse response;
        response.statusCode = 400;
        response.command = "effects.addEffect";
        response.error = api::transport::ApiError{"VALIDATION_ERROR", "bad request", nlohmann::json::object()};

        auto envelope = api::transport::BuildJsonHttpEnvelope(response);
        Check(envelope.value("ok", true) == false,
              "BuildJsonHttpEnvelope should expose ok=false for error responses.", result);
        Check(envelope.contains("error"),
              "BuildJsonHttpEnvelope should retain error payloads.", result);
    }

    return result;
}

} // namespace xLightsDesigner
