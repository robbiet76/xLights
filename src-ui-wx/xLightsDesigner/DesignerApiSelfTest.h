#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "api/handlers/LayoutHandler.h"
#include "api/services/LayoutService.h"
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
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/scene") == std::optional<std::string>("layout.getScene"),
              "EndpointRouter should map layout scene endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/submodels") == std::optional<std::string>("layout.getSubmodels"),
              "EndpointRouter should map layout submodels endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/model-nodes") == std::optional<std::string>("layout.getModelNodes"),
              "EndpointRouter should map layout model-nodes endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/show-directory") == std::optional<std::string>("media.setShowDirectory"),
              "EndpointRouter should map media show-directory mutation endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/request-show-directory-access") == std::optional<std::string>("media.requestShowDirectoryAccess"),
              "EndpointRouter should map media show-directory access request endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/paths/validate-access") == std::optional<std::string>("media.validatePathAccess"),
              "EndpointRouter should map media path access validation endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/media/audio/capabilities") == std::optional<std::string>("media.audio.getCapabilities"),
              "EndpointRouter should map media audio capabilities endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/layout/models/custom") == std::optional<std::string>("layout.createCustomModel"),
              "EndpointRouter should map custom model creation endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/settings") == std::optional<std::string>("sequence.setSettings"),
              "EndpointRouter should map sequence settings mutation endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/timing/add-marks") == std::optional<std::string>("timing.addMarks"),
              "EndpointRouter should map timing add-marks endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/render-samples") == std::optional<std::string>("sequence.getRenderSamples"),
              "EndpointRouter should map render samples endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/capabilities") == std::optional<std::string>("runtime.getCapabilities"),
              "EndpointRouter should map capability discovery endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/channel-map") == std::optional<std::string>("layout.getChannelMap"),
              "EndpointRouter should map layout channel map endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/sequence/data-layers") == std::optional<std::string>("sequence.dataLayers.list"),
              "EndpointRouter should map DataLayer list endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/data-layers/upsert") == std::optional<std::string>("sequence.dataLayers.upsert"),
              "EndpointRouter should map DataLayer upsert endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/export-preview-video") == std::optional<std::string>("sequence.exportPreviewVideo"),
              "EndpointRouter should map preview video export endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/elements/display-order") == std::optional<std::string>("elements.getDisplayOrder"),
              "EndpointRouter should map display-order read endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/elements/display-order") == std::optional<std::string>("elements.setDisplayOrder"),
              "EndpointRouter should map display-order mutation endpoint.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/effects/update").has_value(),
              "EndpointRouter should not expose native effect mutation endpoints.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/sequencing/apply-window-plan").has_value(),
              "EndpointRouter should not expose native effect sequencing endpoints.", result);
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
        std::optional<api::models::CreateCustomModelRequest> capturedRequest;
        api::services::LayoutService service(
            []() { return api::models::LayoutModelsSummary{}; },
            []() { return api::models::LayoutSubmodelsSummary{}; },
            [](const api::models::LayoutModelNodesRequest&) { return api::models::LayoutModelNodesSummary{}; },
            []() { return api::models::LayoutChannelMapSummary{}; },
            []() { return api::models::LayoutSettingsSummary{}; },
            []() { return api::models::LayoutGroupMembershipsSummary{}; },
            [&capturedRequest](const api::models::CreateCustomModelRequest& request) {
                capturedRequest = request;
                return api::models::CreateCustomModelResult{};
            });
        api::handlers::LayoutHandler handler(std::move(service));

        api::transport::ApiRequest request;
        request.command = "layout.createCustomModel";
        request.params = {
            {"name", "SelfTestCustom"},
            {"startChannel", "1"},
            {"layoutGroup", "Default"},
            {"width", "1"},
            {"height", "1"},
            {"depth", "1"},
            {"stringCount", "1"},
            {"positionX", "0"},
            {"positionY", "0"},
            {"dryRun", "true"},
            {"nodes", R"([{"x":0,"y":0,"z":0,"node":1,"string":1}])"}
        };

        const auto response = handler.handleCreateCustomModel(request);
        Check(response.data.value("dryRun", false) == true,
              "LayoutHandler should report dryRun=true when custom model creation is requested in dry-run mode.", result);
        Check(capturedRequest.has_value() && capturedRequest->dryRun,
              "LayoutHandler should pass body dryRun=true through to CreateCustomModelRequest.", result);
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
        response.command = "sequence.open";
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
