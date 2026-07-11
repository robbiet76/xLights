#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "api/handlers/LayoutHandler.h"
#include "api/handlers/MediaHandler.h"
#include "api/services/LayoutService.h"
#include "api/services/MediaService.h"
#include "api/transport/ApiResponse.h"
#include "api/transport/EndpointRouter.h"
#include "api/transport/JsonTransport.h"

namespace xLightsDesigner {

// Self-tests validate route shape, request normalization, and response envelope
// behavior without requiring an open sequence or show folder.
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
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/render-buffer-nodes") == std::optional<std::string>("layout.getRenderBufferNodes"),
              "EndpointRouter should map layout render-buffer-nodes endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/show-directory") == std::optional<std::string>("media.setShowDirectory"),
              "EndpointRouter should map media show-directory mutation endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/request-show-directory-access") == std::optional<std::string>("media.requestShowDirectoryAccess"),
              "EndpointRouter should map media show-directory access request endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/paths/validate-access") == std::optional<std::string>("media.validatePathAccess"),
              "EndpointRouter should map media path access validation endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/media/audio/capabilities") == std::optional<std::string>("media.audio.getCapabilities"),
              "EndpointRouter should map media audio capabilities endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/media/audio/analyze") == std::optional<std::string>("media.audio.analyze"),
              "EndpointRouter should map media audio analysis endpoint.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/layout/models/custom").has_value(),
              "EndpointRouter should not expose layout model creation endpoints.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/settings") == std::optional<std::string>("sequence.setSettings"),
              "EndpointRouter should map sequence settings mutation endpoint.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/timing/add-marks").has_value(),
              "EndpointRouter should not expose timing mark write endpoints.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/timing/ensure-track").has_value(),
              "EndpointRouter should not expose timing track creation endpoints.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/render-samples") == std::optional<std::string>("sequence.getRenderSamples"),
              "EndpointRouter should map render samples endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/capabilities") == std::optional<std::string>("runtime.getCapabilities"),
              "EndpointRouter should map capability discovery endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/channel-map") == std::optional<std::string>("layout.getChannelMap"),
              "EndpointRouter should map layout channel map endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/layout/preview-groups") == std::optional<std::string>("layout.getPreviewGroups"),
              "EndpointRouter should map layout preview-groups endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/sequence/data-layers") == std::optional<std::string>("sequence.dataLayers.list"),
              "EndpointRouter should map DataLayer list endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/data-layers/upsert") == std::optional<std::string>("sequence.dataLayers.upsert"),
              "EndpointRouter should map DataLayer upsert endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/sequence/export-preview-video") == std::optional<std::string>("sequence.exportPreviewVideo"),
              "EndpointRouter should map preview video export endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/elements/display-order") == std::optional<std::string>("elements.getDisplayOrder"),
              "EndpointRouter should map display-order read endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/elements/ensure") == std::optional<std::string>("elements.ensure"),
              "EndpointRouter should map sequence element ensure endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/elements/display-order") == std::optional<std::string>("elements.setDisplayOrder"),
              "EndpointRouter should map display-order mutation endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/elements/selected") == std::optional<std::string>("elements.getSelected"),
              "EndpointRouter should map selected element read endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/elements/selected") == std::optional<std::string>("elements.setSelected"),
              "EndpointRouter should map selected element mutation endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/elements/visibility") == std::optional<std::string>("elements.setVisibility"),
              "EndpointRouter should map element visibility mutation endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/effects") == std::optional<std::string>("effects.list"),
              "EndpointRouter should map native effect list endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/effects/schemas") == std::optional<std::string>("effects.schemas"),
              "EndpointRouter should map native effect schema endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/effects/upsert") == std::optional<std::string>("effects.upsert"),
              "EndpointRouter should map native effect upsert endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/effects/remove") == std::optional<std::string>("effects.remove"),
              "EndpointRouter should map native effect remove endpoint.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/effects/layers") == std::optional<std::string>("effects.layers.list"),
              "EndpointRouter should map native effect layer list endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/effects/layers/ensure") == std::optional<std::string>("effects.layers.ensure"),
              "EndpointRouter should map native effect layer ensure endpoint.", result);
        Check(router.resolve("POST", "/xlightsdesigner/api/effects/layers/remove") == std::optional<std::string>("effects.layers.remove"),
              "EndpointRouter should map native effect layer remove endpoint.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/effects/update").has_value(),
              "EndpointRouter should reject unsupported native effect endpoint names.", result);
        Check(!router.resolve("POST", "/xlightsdesigner/api/sequencing/apply-window-plan").has_value(),
              "EndpointRouter should not expose native effect sequencing endpoints.", result);
        Check(!router.resolve("DELETE", "/xlightsdesigner/api/sequence/open").has_value(),
              "EndpointRouter should reject unsupported method/path pairs.", result);
        Check(router.resolve("GET", "/xlightsdesigner/api/timing/song-structure") == std::optional<std::string>("timing.getSongStructure"),
              "EndpointRouter should map native song structure reads.", result);
    }

    {
        api::services::MediaService mediaService(
            []() {
                api::models::MediaSummary summary;
                summary.sequenceOpen = true;
                summary.mediaContentFingerprint = "xlights-audio-md5:0123456789abcdef";
                return summary;
            },
            []() { return api::models::MediaDirectoriesSummary{}; },
            [](const api::models::MediaShowDirectoryRequest&) { return api::models::MediaShowDirectoryResult{}; },
            [](const api::models::MediaShowDirectoryRequest&) { return api::models::MediaShowDirectoryResult{}; },
            [](const api::models::MediaPathAccessValidationRequest&) { return api::models::MediaPathAccessValidationResult{}; },
            []() { return api::models::MediaAudioCapabilitiesSummary{}; },
            []() { return api::models::MediaAudioAnalysisSummary{}; });
        api::handlers::MediaHandler mediaHandler(std::move(mediaService));
        api::transport::ApiRequest request;
        request.command = "media.getCurrent";

        const auto response = mediaHandler.handleGetCurrent(request);
        Check(response.data.value("mediaContentFingerprint", "") == "xlights-audio-md5:0123456789abcdef",
              "MediaHandler should expose the typed active-audio content fingerprint.", result);
    }

    {
        api::services::LayoutService layoutService(
            []() {
                api::models::LayoutModelsSummary summary;
                api::models::LayoutModelSummary configured;
                configured.name = "Configured Tree";
                configured.controllerCalibration.controllerName = "Front Controller";
                configured.controllerCalibration.controllerPort = 3;
                configured.controllerCalibration.connectionSource = "model_controller_connection";
                configured.controllerCalibration.configuredBrightnessPercent = 42;
                configured.controllerCalibration.brightnessExplicitlySet = true;
                configured.controllerCalibration.brightnessActive = true;
                configured.controllerCalibration.configuredBrightnessSource = "model_port_explicit";
                configured.controllerCalibration.configuredGamma = 2.2;
                configured.controllerCalibration.gammaExplicitlySet = true;
                configured.controllerCalibration.gammaActive = true;
                configured.controllerCalibration.configuredGammaSource = "model_port_explicit";
                configured.controllerCalibration.fullXlightsControlActive = true;
                configured.controllerCalibration.controllerDefaultBrightnessPercent = 80;
                configured.controllerCalibration.controllerDefaultBrightnessSource = "controller_full_control_default";
                configured.controllerCalibration.controllerDefaultGamma = 1.8;
                configured.controllerCalibration.controllerDefaultGammaSource = "controller_full_control_default";
                configured.controllerCalibration.effectiveBrightnessPercent = 42;
                configured.controllerCalibration.effectiveBrightnessSource = "model_port_explicit";
                configured.controllerCalibration.effectiveGamma = 2.2;
                configured.controllerCalibration.effectiveGammaSource = "model_port_explicit";
                summary.models.push_back(std::move(configured));

                api::models::LayoutModelSummary unavailable;
                unavailable.name = "Unassigned Prop";
                summary.models.push_back(std::move(unavailable));
                return summary;
            },
            []() { return api::models::LayoutSubmodelsSummary{}; },
            [](const api::models::LayoutModelNodesRequest&) { return api::models::LayoutModelNodesSummary{}; },
            [](const api::models::LayoutRenderBufferNodesRequest&) { return api::models::LayoutRenderBufferNodesSummary{}; },
            []() { return api::models::LayoutChannelMapSummary{}; },
            []() { return api::models::LayoutSettingsSummary{}; },
            []() { return api::models::LayoutGroupMembershipsSummary{}; },
            []() { return api::models::LayoutPreviewGroupsSummary{}; });
        api::handlers::LayoutHandler layoutHandler(std::move(layoutService));
        api::transport::ApiRequest request;
        request.command = "layout.getModels";

        const auto response = layoutHandler.handleGetModels(request);
        const auto& configured = response.data["models"][0]["controllerCalibration"];
        Check(configured.value("configuredBrightnessPercent", 0) == 42
                  && configured.value("effectiveBrightnessSource", "") == "model_port_explicit",
              "LayoutHandler should expose explicit model/port brightness and provenance.", result);
        Check(configured.value("controllerDefaultBrightnessPercent", 0) == 80
                  && configured.value("controllerDefaultBrightnessSource", "") == "controller_full_control_default",
              "LayoutHandler should expose a resolvable full-control default without replacing an explicit value.", result);
        Check(configured.value("configuredGamma", 0.0) == 2.2
                  && configured.value("effectiveGammaSource", "") == "model_port_explicit",
              "LayoutHandler should expose explicit model/port gamma and provenance.", result);
        Check(configured.value("deploymentMetadataOnly", false)
                  && !configured.value("previewApplicationProven", true),
              "LayoutHandler should identify controller calibration as unproven preview metadata.", result);

        const auto& unavailable = response.data["models"][1]["controllerCalibration"];
        Check(unavailable["controllerName"].is_null()
                  && unavailable["controllerPort"].is_null()
                  && unavailable["configuredBrightnessPercent"].is_null()
                  && unavailable["effectiveBrightnessPercent"].is_null()
                  && unavailable["configuredGamma"].is_null()
                  && unavailable["effectiveGamma"].is_null(),
              "LayoutHandler should serialize unavailable calibration as null rather than implicit defaults.", result);
        Check(unavailable.value("connectionSource", "") == "unavailable"
                  && unavailable.value("controllerDefaultBrightnessSource", "") == "controller_not_resolved"
                  && unavailable.value("effectiveBrightnessSource", "") == "unavailable",
              "LayoutHandler should expose unavailable calibration provenance.", result);
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
