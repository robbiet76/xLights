#pragma once

#include <optional>

#include "../handlers/DataLayerHandler.h"
#include "../handlers/ElementHandler.h"
#include "../handlers/EffectHandler.h"
#include "../handlers/LayoutHandler.h"
#include "../handlers/MediaHandler.h"
#include "../handlers/RuntimeHandler.h"
#include "../handlers/SequenceHandler.h"
#include "../handlers/TimingHandler.h"
#include "ApiRequest.h"
#include "ApiResponse.h"

namespace xLightsDesigner::api::transport {

// Dispatches normalized commands to cohesive handler groups. A command must be
// present in EndpointRouter and here to be externally reachable.
class RequestRouter {
public:
    RequestRouter(handlers::RuntimeHandler runtimeHandler,
                  handlers::SequenceHandler sequenceHandler,
                  handlers::DataLayerHandler dataLayerHandler,
                  handlers::EffectHandler effectHandler,
                  handlers::TimingHandler timingHandler,
                  handlers::MediaHandler mediaHandler,
                  handlers::LayoutHandler layoutHandler,
                  handlers::ElementHandler elementHandler)
        : _runtimeHandler(std::move(runtimeHandler)),
          _sequenceHandler(std::move(sequenceHandler)),
          _dataLayerHandler(std::move(dataLayerHandler)),
          _effectHandler(std::move(effectHandler)),
          _timingHandler(std::move(timingHandler)),
          _mediaHandler(std::move(mediaHandler)),
          _layoutHandler(std::move(layoutHandler)),
          _elementHandler(std::move(elementHandler)) {}

    [[nodiscard]] std::optional<ApiResponse> route(const ApiRequest& request) const {
        // Runtime.
        if (request.command == "health.get") return _runtimeHandler.handleHealth(request);
        if (request.command == "runtime.getCapabilities") return _runtimeHandler.handleGetCapabilities(request);
        if (request.command == "jobs.get") return _runtimeHandler.handleGetJob(request);

        // Sequence lifecycle and render/readback operations.
        if (request.command == "sequence.getOpen") return _sequenceHandler.handleGetOpen(request);
        if (request.command == "sequence.getRevision") return _sequenceHandler.handleGetRevision(request);
        if (request.command == "sequence.getState") return _sequenceHandler.handleGetState(request);
        if (request.command == "sequence.getSettings") return _sequenceHandler.handleGetSettings(request);
        if (request.command == "sequence.setSettings") return _sequenceHandler.handleSetSettings(request);
        if (request.command == "sequence.open") return _sequenceHandler.handleOpen(request);
        if (request.command == "sequence.create") return _sequenceHandler.handleCreate(request);
        if (request.command == "sequence.save") return _sequenceHandler.handleSave(request);
        if (request.command == "sequence.close") return _sequenceHandler.handleClose(request);
        if (request.command == "sequence.focus") return _sequenceHandler.handleFocus(request);
        if (request.command == "sequence.renderCurrent") return _sequenceHandler.handleRenderCurrent(request);
        if (request.command == "sequence.saveFinalFseq") return _sequenceHandler.handleSaveFinalFseq(request);
        if (request.command == "sequence.check") return _sequenceHandler.handleCheck(request);
        if (request.command == "sequence.exportPreviewVideo") return _sequenceHandler.handleExportPreviewVideo(request);
        if (request.command == "sequence.getRenderSamples") return _sequenceHandler.handleGetRenderSamples(request);
        if (request.command == "sequence.getFinalFseq") return _sequenceHandler.handleGetFinalFseq(request);
        if (request.command == "sequence.getSyncHealth") return _sequenceHandler.handleGetSyncHealth(request);

        // DataLayer management for generated XLD FSEQ files.
        if (request.command == "sequence.dataLayers.list") return _dataLayerHandler.handleList(request);
        if (request.command == "sequence.dataLayers.upsert") return _dataLayerHandler.handleUpsert(request);
        if (request.command == "sequence.dataLayers.remove") return _dataLayerHandler.handleRemove(request);
        if (request.command == "sequence.dataLayers.reorder") return _dataLayerHandler.handleReorder(request);
        if (request.command == "sequence.dataLayers.validate") return _dataLayerHandler.handleValidate(request);

        // Native xLights effect and layer control.
        if (request.command == "effects.schemas") return _effectHandler.handleListSchemas(request);
        if (request.command == "effects.list") return _effectHandler.handleListEffects(request);
        if (request.command == "effects.upsert") return _effectHandler.handleUpsertEffect(request);
        if (request.command == "effects.remove") return _effectHandler.handleRemoveEffect(request);
        if (request.command == "effects.layers.list") return _effectHandler.handleListLayers(request);
        if (request.command == "effects.layers.ensure") return _effectHandler.handleEnsureLayer(request);
        if (request.command == "effects.layers.remove") return _effectHandler.handleRemoveLayer(request);

        // Timing tracks and marks.
        if (request.command == "timing.getTracks") return _timingHandler.handleGetTracks(request);
        if (request.command == "timing.getMarks") return _timingHandler.handleGetMarks(request);

        // Media, show-folder, and path-access operations.
        if (request.command == "media.getCurrent") return _mediaHandler.handleGetCurrent(request);
        if (request.command == "media.getDirectories") return _mediaHandler.handleGetDirectories(request);
        if (request.command == "media.setShowDirectory") return _mediaHandler.handleSetShowDirectory(request);
        if (request.command == "media.requestShowDirectoryAccess") return _mediaHandler.handleRequestShowDirectoryAccess(request);
        if (request.command == "media.validatePathAccess") return _mediaHandler.handleValidatePathAccess(request);
        if (request.command == "media.audio.getCapabilities") return _mediaHandler.handleGetAudioCapabilities(request);
        if (request.command == "media.audio.analyze") return _mediaHandler.handleAnalyzeAudio(request);

        // Read-only layout/display structure.
        if (request.command == "layout.getModels") return _layoutHandler.handleGetModels(request);
        if (request.command == "layout.getSubmodels") return _layoutHandler.handleGetSubmodels(request);
        if (request.command == "layout.getModelNodes") return _layoutHandler.handleGetModelNodes(request);
        if (request.command == "layout.getRenderBufferNodes") return _layoutHandler.handleGetRenderBufferNodes(request);
        if (request.command == "layout.getChannelMap") return _layoutHandler.handleGetChannelMap(request);
        if (request.command == "layout.getScene") return _layoutHandler.handleGetScene(request);
        if (request.command == "layout.getSettings") return _layoutHandler.handleGetSettings(request);
        if (request.command == "layout.getGroupMembers") return _layoutHandler.handleGetGroupMembers(request);

        // Display element row/order state.
        if (request.command == "elements.getSummary") return _elementHandler.handleGetSummary(request);
        if (request.command == "elements.ensure") return _elementHandler.handleEnsureSequenceElements(request);
        if (request.command == "elements.getDisplayOrder") return _elementHandler.handleGetDisplayOrder(request);
        if (request.command == "elements.setDisplayOrder") return _elementHandler.handleSetDisplayOrder(request);
        if (request.command == "elements.getSelected") return _elementHandler.handleGetSelectedDisplayElements(request);
        if (request.command == "elements.setSelected") return _elementHandler.handleSetSelectedDisplayElements(request);

        return std::nullopt;
    }

private:
    handlers::RuntimeHandler _runtimeHandler;
    handlers::SequenceHandler _sequenceHandler;
    handlers::DataLayerHandler _dataLayerHandler;
    handlers::EffectHandler _effectHandler;
    handlers::TimingHandler _timingHandler;
    handlers::MediaHandler _mediaHandler;
    handlers::LayoutHandler _layoutHandler;
    handlers::ElementHandler _elementHandler;
};

} // namespace xLightsDesigner::api::transport
