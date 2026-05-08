#pragma once

/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <string>

#include <log.h>

#include "DesignerApiHost.h"
#include "DesignerDiagnostics.h"
#include "DesignerApiListener.h"
#include "DesignerApiRuntime.h"
#include "DesignerApiSelfTest.h"
#include "DesignerApiSmoke.h"
#include "api/handlers/EffectHandler.h"
#include "api/handlers/ElementHandler.h"
#include "api/handlers/LayoutHandler.h"
#include "api/handlers/MediaHandler.h"
#include "api/handlers/SequenceHandler.h"
#include "api/handlers/SequencingHandler.h"
#include "api/handlers/TimingHandler.h"
#include "api/parsing/RequestParser.h"
#include "api/services/EffectService.h"
#include "api/services/ElementService.h"
#include "api/services/LayoutService.h"
#include "api/services/MediaService.h"
#include "api/services/SequenceService.h"
#include "api/services/SequencingService.h"
#include "api/services/TimingService.h"
#include "api/transport/ApiResponse.h"
#include "api/transport/EndpointRouter.h"
#include "api/transport/JsonTransport.h"
#include "api/transport/RequestRouter.h"

class xLightsFrame;

namespace xLightsDesigner {
inline std::optional<api::transport::ApiResponse> HandleDesignerApiEndpoint(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& queryParams,
    const nlohmann::json& body,
    const std::string& requestId);

namespace detail {
inline xLightsFrame*& IntegrationFrame() {
    static xLightsFrame* frame = nullptr;
    return frame;
}

inline bool& IntegrationInitialized() {
    static bool initialized = false;
    return initialized;
}

inline bool& RuntimeActivated() {
    static bool activated = false;
    return activated;
}

} // namespace detail

inline bool IsTruthyFlagValue(const char* value) {
    if (value == nullptr) {
        return false;
    }
    const std::string enabled(value);
    return enabled == "1" || enabled == "true" || enabled == "TRUE" || enabled == "yes" || enabled == "YES";
}

inline bool FileExistsAndNotEmpty(const std::string& path) {
    std::ifstream file(path);
    return file.good() && file.peek() != std::ifstream::traits_type::eof();
}

inline bool IsDesignerFlagEnabledFromFile(const char* fileName) {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        return false;
    }

    const std::string userBase = std::string(home) + "/Library/Application Support/xLightsDesigner/";
    if (FileExistsAndNotEmpty(userBase + fileName)) {
        return true;
    }

    const std::string sandboxBase = std::string(home) + "/Library/Containers/org.xlights/Data/Library/Application Support/xLightsDesigner/";
    return FileExistsAndNotEmpty(sandboxBase + fileName);
}

inline bool IsDesignerSmokeEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_SMOKE")) || IsDesignerFlagEnabledFromFile("smoke-enabled");
}

inline bool IsDesignerSelfTestEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_SELF_TEST")) || IsDesignerFlagEnabledFromFile("self-test-enabled");
}

inline bool IsDesignerIntegrationEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_ENABLED")) || IsDesignerFlagEnabledFromFile("integration-enabled");
}

inline void InitializeDesignerIntegration(xLightsFrame* frame) {
    if (detail::IntegrationInitialized()) {
        return;
    }

    detail::IntegrationFrame() = frame;
    detail::IntegrationInitialized() = true;

    const bool integrationEnabled = IsDesignerIntegrationEnabled();
    const bool selfTestEnabled = IsDesignerSelfTestEnabled();
    const bool smokeEnabled = IsDesignerSmokeEnabled();
    const bool shouldActivate = integrationEnabled || selfTestEnabled || smokeEnabled;

    AppendDesignerDiagnostic(std::string("InitializeDesignerIntegration integration=") + (integrationEnabled ? "1" : "0") + " selfTest=" + (selfTestEnabled ? "1" : "0") + " smoke=" + (smokeEnabled ? "1" : "0") + " shouldActivate=" + (shouldActivate ? "1" : "0"));

    if (integrationEnabled) {
        spdlog::info("xLightsDesigner integration initialized.");
    } else {
        spdlog::info("xLightsDesigner integration available but inactive.");
    }

    if (selfTestEnabled) {
        const auto selfTest = RunDesignerApiSelfTests();
        if (selfTest.ok()) {
            spdlog::info("xLightsDesigner self-tests passed ({} checks).", selfTest.passed);
        } else {
            spdlog::error("xLightsDesigner self-tests failed ({} passed, {} failed).", selfTest.passed, selfTest.failed);
            for (const auto& failure : selfTest.failures) {
                spdlog::error("xLightsDesigner self-test failure: {}", failure);
            }
        }
    }

    if (!shouldActivate) {
        return;
    }

    AppendDesignerDiagnostic("InitializeDesignerIntegration starting runtime");
    StartDesignerApiRuntime();
    detail::RuntimeActivated() = true;

    if (integrationEnabled) {
        AppendDesignerDiagnostic("InitializeDesignerIntegration starting listener");
        const bool listenerStarted = StartDesignerApiListener([](
            const std::string& method,
            const std::string& path,
            const std::map<std::string, std::string>& queryParams,
            const nlohmann::json& body,
            const std::string& requestId) {
            return HandleDesignerApiEndpoint(method, path, queryParams, body, requestId);
        });
        AppendDesignerDiagnostic(std::string("InitializeDesignerIntegration listenerStarted=") + (listenerStarted ? "1" : "0"));
    }

    if (smokeEnabled) {
        const auto smoke = RunDesignerApiSmoke();
        spdlog::info("xLightsDesigner smoke complete ({} passed, {} failed).", smoke.passed, smoke.failed);
        for (const auto& failure : smoke.failures) {
            spdlog::error("xLightsDesigner smoke failure: {}", failure);
        }
    }
}

inline void NotifyDesignerAppReady() {
    if (!detail::RuntimeActivated()) {
        AppendDesignerDiagnostic("NotifyDesignerAppReady skipped runtimeInactive");
        return;
    }
    AppendDesignerDiagnostic("NotifyDesignerAppReady marking ready");
    MarkDesignerApiAppReady();
}

inline void ShutdownDesignerIntegration() {
    if (!detail::IntegrationInitialized()) {
        return;
    }

    if (detail::RuntimeActivated()) {
        if (IsDesignerIntegrationEnabled()) {
            StopDesignerApiListener();
            spdlog::info("xLightsDesigner integration shutdown.");
        }

        StopDesignerApiRuntime();
        detail::RuntimeActivated() = false;
    }
    detail::IntegrationFrame() = nullptr;
    detail::IntegrationInitialized() = false;
}

inline xLightsFrame* GetDesignerIntegrationFrame() {
    return detail::IntegrationFrame();
}

inline std::optional<api::transport::ApiResponse> HandleDesignerApiRequest(
    const std::string& command,
    const std::map<std::string, std::string>& params,
    const std::string& requestId = {}) {
    xLightsFrame* frame = GetDesignerIntegrationFrame();
    if (frame == nullptr) {
        return std::nullopt;
    }

    auto host = std::make_shared<DesignerApiHost>(frame);
    api::services::SequenceService sequenceService(
        [host]() { return host->readOpenSequence(); },
        [host]() { return host->readSequenceSettings(); },
        [host](const api::models::SequenceOpenRequest& request) { return host->openSequence(request); },
        [host](const api::models::SequenceCreateRequest& request) { return host->createSequence(request); },
        [host](const api::models::SequenceSettingsUpdateRequest& request) { return host->updateSequenceSettings(request); },
        [host]() { return host->saveSequence(); },
        [host]() { return host->closeSequence(); },
        [host]() { return host->renderCurrentSequence(); },
        [host]() { return host->checkSequence(); },
        [host](const api::models::SequencePreviewVideoExportRequest& request) { return host->exportPreviewVideo(request); },
        [host](const api::models::SequenceRenderSamplesRequest& request) { return host->readRenderedSamples(request); });
    api::services::TimingService timingService(
        [host]() { return host->readTimingTracks(); },
        [host](const api::models::TimingMarksRequest& request) { return host->readTimingMarks(request); },
        [host](const api::models::EnsureTimingTrackRequest& request) { return host->ensureTimingTrack(request); },
        [host](const api::models::AddTimingMarksRequest& request) { return host->addTimingMarks(request); });
    api::services::MediaService mediaService(
        [host]() { return host->readCurrentMedia(); },
        [host]() { return host->readMediaDirectories(); },
        [host](const api::models::MediaShowDirectoryRequest& request) { return host->setShowDirectory(request); },
        [host](const api::models::MediaShowDirectoryRequest& request) { return host->requestShowDirectoryAccess(request); });
    api::services::LayoutService layoutService(
        [host]() { return host->readLayoutModels(); },
        [host]() { return host->readLayoutSubmodels(); },
        [host](const api::models::LayoutModelNodesRequest& request) { return host->readLayoutModelNodes(request); },
        [host]() { return host->readLayoutSettings(); },
        [host]() { return host->readLayoutGroupMemberships(); },
        [host](const api::models::CreateCustomModelRequest& request) { return host->createCustomModel(request); });
    api::services::ElementService elementService(
        [host]() { return host->readElements(); },
        [host]() { return host->readDisplayElementOrder(); },
        [host](const api::models::SetDisplayElementOrderRequest& request) { return host->setDisplayElementOrder(request); });
    api::services::EffectService effectService(
        [host](const api::models::EffectWindowRequest& request) { return host->readEffectsWindow(request); },
        [host](const api::models::AddEffectRequest& request) { return host->addEffect(request); },
        [host](const api::models::ClearEffectWindowRequest& request) { return host->clearEffectsWindow(request); },
        [host](const api::models::ApplyEffectBatchRequest& request) { return host->applyEffectBatch(request); },
        [host](const api::models::CloneEffectsRequest& request) { return host->cloneEffects(request); },
        [host](const api::models::UpdateEffectRequest& request) { return host->updateEffect(request); },
        [host](const api::models::DeleteEffectsRequest& request) { return host->deleteEffects(request); },
        [host](const api::models::DeleteEffectLayerRequest& request) { return host->deleteEffectLayer(request); },
        [host](const api::models::ReorderEffectLayerRequest& request) { return host->reorderEffectLayer(request); },
        [host](const api::models::CompactEffectLayersRequest& request) { return host->compactEffectLayers(request); });
    api::services::SequencingService sequencingService(timingService, effectService);
    api::handlers::RuntimeHandler runtimeHandler([host]() { return host->readModalState(); });
    api::handlers::SequenceHandler sequenceHandler(std::move(sequenceService));
    api::handlers::TimingHandler timingHandler(std::move(timingService));
    api::handlers::MediaHandler mediaHandler(std::move(mediaService));
    api::handlers::LayoutHandler layoutHandler(std::move(layoutService));
    api::handlers::ElementHandler elementHandler(std::move(elementService));
    api::handlers::EffectHandler effectHandler(std::move(effectService));
    api::handlers::SequencingHandler sequencingHandler(std::move(sequencingService));
    api::transport::RequestRouter router(std::move(runtimeHandler), std::move(sequenceHandler), std::move(timingHandler), std::move(mediaHandler), std::move(layoutHandler), std::move(elementHandler), std::move(effectHandler), std::move(sequencingHandler));

    return router.route(api::parsing::ParseRequest(command, params, requestId));
}

inline std::optional<api::transport::ApiResponse> HandleDesignerApiEndpoint(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& queryParams,
    const nlohmann::json& body,
    const std::string& requestId) {
    api::transport::EndpointRouter endpointRouter;
    const auto command = endpointRouter.resolve(method, path);
    if (!command.has_value()) {
        return std::nullopt;
    }

    return HandleDesignerApiRequest(
        *command,
        api::transport::MergeRequestParams(queryParams, body),
        requestId);
}

inline std::optional<nlohmann::json> HandleDesignerApiEndpointJson(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& queryParams = {},
    const nlohmann::json& body = nlohmann::json::object(),
    const std::string& requestId = {}) {
    auto response = HandleDesignerApiEndpoint(method, path, queryParams, body, requestId);
    if (!response.has_value()) {
        return std::nullopt;
    }
    return api::transport::BuildJsonHttpEnvelope(*response);
}
} // namespace xLightsDesigner
