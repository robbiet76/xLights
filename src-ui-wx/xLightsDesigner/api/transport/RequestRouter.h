#pragma once

#include <optional>

#include "../handlers/EffectHandler.h"
#include "../handlers/ElementHandler.h"
#include "../handlers/LayoutHandler.h"
#include "../handlers/MediaHandler.h"
#include "../handlers/RuntimeHandler.h"
#include "../handlers/SequenceHandler.h"
#include "../handlers/SequencingHandler.h"
#include "../handlers/TimingHandler.h"
#include "ApiRequest.h"
#include "ApiResponse.h"

namespace xLightsDesigner::api::transport {

class RequestRouter {
public:
    RequestRouter(handlers::RuntimeHandler runtimeHandler,
                  handlers::SequenceHandler sequenceHandler,
                  handlers::TimingHandler timingHandler,
                  handlers::MediaHandler mediaHandler,
                  handlers::LayoutHandler layoutHandler,
                  handlers::ElementHandler elementHandler,
                  handlers::EffectHandler effectHandler,
                  handlers::SequencingHandler sequencingHandler)
        : _runtimeHandler(std::move(runtimeHandler)),
          _sequenceHandler(std::move(sequenceHandler)),
          _timingHandler(std::move(timingHandler)),
          _mediaHandler(std::move(mediaHandler)),
          _layoutHandler(std::move(layoutHandler)),
          _elementHandler(std::move(elementHandler)),
          _effectHandler(std::move(effectHandler)),
          _sequencingHandler(std::move(sequencingHandler)) {}

    [[nodiscard]] std::optional<ApiResponse> route(const ApiRequest& request) const {
        if (request.command == "health.get") {
            return _runtimeHandler.handleHealth(request);
        }
        if (request.command == "jobs.get") {
            return _runtimeHandler.handleGetJob(request);
        }
        if (request.command == "metadata.effects.status") {
            return _runtimeHandler.handleMetadataStatus(request);
        }
        if (request.command == "sequence.getOpen") {
            return _sequenceHandler.handleGetOpen(request);
        }
        if (request.command == "sequence.getRevision") {
            return _sequenceHandler.handleGetRevision(request);
        }
        if (request.command == "sequence.getSettings") {
            return _sequenceHandler.handleGetSettings(request);
        }
        if (request.command == "sequence.open") {
            return _sequenceHandler.handleOpen(request);
        }
        if (request.command == "sequence.create") {
            return _sequenceHandler.handleCreate(request);
        }
        if (request.command == "sequence.save") {
            return _sequenceHandler.handleSave(request);
        }
        if (request.command == "sequence.close") {
            return _sequenceHandler.handleClose(request);
        }
        if (request.command == "sequence.renderCurrent") {
            return _sequenceHandler.handleRenderCurrent(request);
        }
        if (request.command == "sequence.getRenderSamples") {
            return _sequenceHandler.handleGetRenderSamples(request);
        }
        if (request.command == "timing.getTracks") {
            return _timingHandler.handleGetTracks(request);
        }
        if (request.command == "timing.getMarks") {
            return _timingHandler.handleGetMarks(request);
        }
        if (request.command == "timing.ensureTrack") {
            return _timingHandler.handleEnsureTrack(request);
        }
        if (request.command == "timing.addMarks") {
            return _timingHandler.handleAddMarks(request);
        }
        if (request.command == "media.getCurrent") {
            return _mediaHandler.handleGetCurrent(request);
        }
        if (request.command == "media.getDirectories") {
            return _mediaHandler.handleGetDirectories(request);
        }
        if (request.command == "layout.getModels") {
            return _layoutHandler.handleGetModels(request);
        }
        if (request.command == "layout.getScene") {
            return _layoutHandler.handleGetScene(request);
        }
        if (request.command == "layout.getSettings") {
            return _layoutHandler.handleGetSettings(request);
        }
        if (request.command == "layout.getGroupMembers") {
            return _layoutHandler.handleGetGroupMembers(request);
        }
        if (request.command == "elements.getSummary") {
            return _elementHandler.handleGetSummary(request);
        }
        if (request.command == "effects.getWindow") {
            return _effectHandler.handleGetWindow(request);
        }
        if (request.command == "effects.addEffect") {
            return _effectHandler.handleAddEffect(request);
        }
        if (request.command == "effects.applyBatch") {
            return _effectHandler.handleApplyBatch(request);
        }
        if (request.command == "effects.clearWindow") {
            return _effectHandler.handleClearWindow(request);
        }
        if (request.command == "sequencing.applyWindowPlan") {
            return _sequencingHandler.handleApplyWindowPlan(request);
        }
        if (request.command == "sequencing.applyBatchPlan") {
            return _sequencingHandler.handleApplyBatchPlan(request);
        }
        return std::nullopt;
    }

private:
    handlers::RuntimeHandler _runtimeHandler;
    handlers::SequenceHandler _sequenceHandler;
    handlers::TimingHandler _timingHandler;
    handlers::MediaHandler _mediaHandler;
    handlers::LayoutHandler _layoutHandler;
    handlers::ElementHandler _elementHandler;
    handlers::EffectHandler _effectHandler;
    handlers::SequencingHandler _sequencingHandler;
};

} // namespace xLightsDesigner::api::transport
