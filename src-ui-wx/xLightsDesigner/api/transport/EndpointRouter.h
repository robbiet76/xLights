#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xLightsDesigner::api::transport {

class EndpointRouter {
public:
    [[nodiscard]] std::optional<std::string> resolve(const std::string& method, const std::string& path) const {
        if (method == "GET" && path == "/xlightsdesigner/api/health") return "health.get";
        if (method == "GET" && path == "/xlightsdesigner/api/capabilities") return "runtime.getCapabilities";
        if (method == "GET" && path == "/xlightsdesigner/api/jobs/get") return "jobs.get";
        if (method == "GET" && path == "/xlightsdesigner/api/metadata/effects/status") return "metadata.effects.status";

        if (method == "GET" && path == "/xlightsdesigner/api/sequence/open") return "sequence.getOpen";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/revision") return "sequence.getRevision";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/state") return "sequence.getState";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/settings") return "sequence.getSettings";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/settings") return "sequence.setSettings";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/open") return "sequence.open";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/create") return "sequence.create";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/save") return "sequence.save";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/close") return "sequence.close";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/focus") return "sequence.focus";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/render-current") return "sequence.renderCurrent";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/save-final-fseq") return "sequence.saveFinalFseq";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/check") return "sequence.check";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/export-preview-video") return "sequence.exportPreviewVideo";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/render-samples") return "sequence.getRenderSamples";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/final-fseq") return "sequence.getFinalFseq";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/sync-health") return "sequence.getSyncHealth";
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/data-layers") return "sequence.dataLayers.list";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/upsert") return "sequence.dataLayers.upsert";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/remove") return "sequence.dataLayers.remove";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/reorder") return "sequence.dataLayers.reorder";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/validate") return "sequence.dataLayers.validate";

        if (method == "GET" && path == "/xlightsdesigner/api/timing/tracks") return "timing.getTracks";
        if (method == "GET" && path == "/xlightsdesigner/api/timing/marks") return "timing.getMarks";
        if (method == "POST" && path == "/xlightsdesigner/api/timing/ensure-track") return "timing.ensureTrack";
        if (method == "POST" && path == "/xlightsdesigner/api/timing/add-marks") return "timing.addMarks";

        if (method == "GET" && path == "/xlightsdesigner/api/media/current") return "media.getCurrent";
        if (method == "GET" && path == "/xlightsdesigner/api/media/directories") return "media.getDirectories";
        if (method == "POST" && path == "/xlightsdesigner/api/media/show-directory") return "media.setShowDirectory";
        if (method == "POST" && path == "/xlightsdesigner/api/media/request-show-directory-access") return "media.requestShowDirectoryAccess";
        if (method == "POST" && path == "/xlightsdesigner/api/media/paths/validate-access") return "media.validatePathAccess";
        if (method == "GET" && path == "/xlightsdesigner/api/media/audio/capabilities") return "media.audio.getCapabilities";

        if (method == "GET" && path == "/xlightsdesigner/api/layout/models") return "layout.getModels";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/submodels") return "layout.getSubmodels";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/model-nodes") return "layout.getModelNodes";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/channel-map") return "layout.getChannelMap";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/scene") return "layout.getScene";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/settings") return "layout.getSettings";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/group-members") return "layout.getGroupMembers";
        if (method == "POST" && path == "/xlightsdesigner/api/layout/models/custom") return "layout.createCustomModel";
        if (method == "GET" && path == "/xlightsdesigner/api/elements/summary") return "elements.getSummary";
        if (method == "GET" && path == "/xlightsdesigner/api/elements/display-order") return "elements.getDisplayOrder";
        if (method == "POST" && path == "/xlightsdesigner/api/elements/display-order") return "elements.setDisplayOrder";

        if (method == "GET" && path == "/xlightsdesigner/api/effects/window") return "effects.getWindow";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/add-effect") return "effects.addEffect";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/apply-batch") return "effects.applyBatch";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/clone") return "effects.clone";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/clear-window") return "effects.clearWindow";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/update") return "effects.update";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/delete") return "effects.delete";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/delete-layer") return "effects.deleteLayer";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/reorder-layer") return "effects.reorderLayer";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/compact-layers") return "effects.compactLayers";

        if (method == "POST" && path == "/xlightsdesigner/api/sequencing/apply-window-plan") return "sequencing.applyWindowPlan";
        if (method == "POST" && path == "/xlightsdesigner/api/sequencing/apply-batch-plan") return "sequencing.applyBatchPlan";
        return std::nullopt;
    }
};

} // namespace xLightsDesigner::api::transport
