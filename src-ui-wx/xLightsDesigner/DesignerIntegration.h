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
#include "api/handlers/DataLayerHandler.h"
#include "api/handlers/ElementHandler.h"
#include "api/handlers/LayoutHandler.h"
#include "api/handlers/MediaHandler.h"
#include "api/handlers/SequenceHandler.h"
#include "api/handlers/TimingHandler.h"
#include "api/parsing/RequestParser.h"
#include "api/services/DataLayerService.h"
#include "api/services/ElementService.h"
#include "api/services/LayoutService.h"
#include "api/services/MediaService.h"
#include "api/services/SequenceService.h"
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

inline bool IsDesignerSmokeEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_SMOKE"));
}

inline bool IsDesignerSelfTestEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_SELF_TEST"));
}

inline bool IsDesignerIntegrationEnabled() {
    return IsTruthyFlagValue(std::getenv("XLIGHTS_DESIGNER_ENABLED"));
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
        [host]() { return host->focusSequence(); },
        [host]() { return host->renderCurrentSequence(); },
        [host]() { return host->checkSequence(); },
        [host](const api::models::SequencePreviewVideoExportRequest& request) { return host->exportPreviewVideo(request); },
        [host](const api::models::SequenceRenderSamplesRequest& request) { return host->readRenderedSamples(request); },
        [host]() { return host->readFinalFseqState(); },
        [host]() { return host->readSyncHealth(); });
    api::services::DataLayerService dataLayerService(
        [host]() { return host->readDataLayers(); },
        [host](const api::models::DataLayerUpsertRequest& request) { return host->upsertDataLayer(request); },
        [host](const api::models::DataLayerRemoveRequest& request) { return host->removeDataLayer(request); },
        [host](const api::models::DataLayerReorderRequest& request) { return host->reorderDataLayer(request); },
        [host](const api::models::DataLayerValidateRequest& request) { return host->validateDataLayer(request); });
    api::services::TimingService timingService(
        [host]() { return host->readTimingTracks(); },
        [host](const api::models::TimingMarksRequest& request) { return host->readTimingMarks(request); },
        [host](const api::models::EnsureTimingTrackRequest& request) { return host->ensureTimingTrack(request); },
        [host](const api::models::AddTimingMarksRequest& request) { return host->addTimingMarks(request); });
    api::services::MediaService mediaService(
        [host]() { return host->readCurrentMedia(); },
        [host]() { return host->readMediaDirectories(); },
        [host](const api::models::MediaShowDirectoryRequest& request) { return host->setShowDirectory(request); },
        [host](const api::models::MediaShowDirectoryRequest& request) { return host->requestShowDirectoryAccess(request); },
        [host](const api::models::MediaPathAccessValidationRequest& request) { return host->validateMediaPathAccess(request); },
        [host]() { return host->readAudioCapabilities(); });
    api::services::LayoutService layoutService(
        [host]() { return host->readLayoutModels(); },
        [host]() { return host->readLayoutSubmodels(); },
        [host](const api::models::LayoutModelNodesRequest& request) { return host->readLayoutModelNodes(request); },
        [host]() { return host->readLayoutChannelMap(); },
        [host]() { return host->readLayoutSettings(); },
        [host]() { return host->readLayoutGroupMemberships(); },
        [host](const api::models::CreateCustomModelRequest& request) { return host->createCustomModel(request); });
    api::services::ElementService elementService(
        [host]() { return host->readElements(); },
        [host]() { return host->readDisplayElementOrder(); },
        [host](const api::models::SetDisplayElementOrderRequest& request) { return host->setDisplayElementOrder(request); });
    api::handlers::RuntimeHandler runtimeHandler([host]() { return host->readModalState(); });
    api::handlers::SequenceHandler sequenceHandler(std::move(sequenceService));
    api::handlers::DataLayerHandler dataLayerHandler(std::move(dataLayerService));
    api::handlers::TimingHandler timingHandler(std::move(timingService));
    api::handlers::MediaHandler mediaHandler(std::move(mediaService));
    api::handlers::LayoutHandler layoutHandler(std::move(layoutService));
    api::handlers::ElementHandler elementHandler(std::move(elementService));
    api::transport::RequestRouter router(std::move(runtimeHandler), std::move(sequenceHandler), std::move(dataLayerHandler), std::move(timingHandler), std::move(mediaHandler), std::move(layoutHandler), std::move(elementHandler));

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
