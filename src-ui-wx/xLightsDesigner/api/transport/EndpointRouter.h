#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xLightsDesigner::api::transport {

// Maps the stable HTTP surface to internal command names. Keeping this table
// separate from RequestRouter makes the externally supported API easy to audit.
class EndpointRouter {
public:
    [[nodiscard]] std::optional<std::string> resolve(const std::string& method, const std::string& path) const {
        // Runtime and job state.
        if (method == "GET" && path == "/xlightsdesigner/api/health") return "health.get";
        if (method == "GET" && path == "/xlightsdesigner/api/capabilities") return "runtime.getCapabilities";
        if (method == "GET" && path == "/xlightsdesigner/api/jobs/get") return "jobs.get";

        // Sequence lifecycle, render feedback, and final-output sync.
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

        // Generated AI FSEQ files enter xLights through DataLayers when direct
        // channel readback is needed.
        if (method == "GET" && path == "/xlightsdesigner/api/sequence/data-layers") return "sequence.dataLayers.list";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/upsert") return "sequence.dataLayers.upsert";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/remove") return "sequence.dataLayers.remove";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/reorder") return "sequence.dataLayers.reorder";
        if (method == "POST" && path == "/xlightsdesigner/api/sequence/data-layers/validate") return "sequence.dataLayers.validate";

        // Native xLights effect and layer control for XLD-authored sequences.
        if (method == "GET" && path == "/xlightsdesigner/api/effects/schemas") return "effects.schemas";
        if (method == "GET" && path == "/xlightsdesigner/api/effects") return "effects.list";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/upsert") return "effects.upsert";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/remove") return "effects.remove";
        if (method == "GET" && path == "/xlightsdesigner/api/effects/layers") return "effects.layers.list";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/layers/ensure") return "effects.layers.ensure";
        if (method == "POST" && path == "/xlightsdesigner/api/effects/layers/remove") return "effects.layers.remove";

        // Timing tracks are owned by xLights and read by XLD.
        if (method == "GET" && path == "/xlightsdesigner/api/timing/tracks") return "timing.getTracks";
        if (method == "GET" && path == "/xlightsdesigner/api/timing/marks") return "timing.getMarks";

        // Media and show-folder operations are the project-to-xLights bridge.
        if (method == "GET" && path == "/xlightsdesigner/api/media/current") return "media.getCurrent";
        if (method == "GET" && path == "/xlightsdesigner/api/media/directories") return "media.getDirectories";
        if (method == "POST" && path == "/xlightsdesigner/api/media/show-directory") return "media.setShowDirectory";
        if (method == "POST" && path == "/xlightsdesigner/api/media/request-show-directory-access") return "media.requestShowDirectoryAccess";
        if (method == "POST" && path == "/xlightsdesigner/api/media/paths/validate-access") return "media.validatePathAccess";
        if (method == "GET" && path == "/xlightsdesigner/api/media/audio/capabilities") return "media.audio.getCapabilities";
        if (method == "POST" && path == "/xlightsdesigner/api/media/audio/analyze") return "media.audio.analyze";

        // Layout endpoints are read-only display discovery for generation.
        if (method == "GET" && path == "/xlightsdesigner/api/layout/models") return "layout.getModels";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/submodels") return "layout.getSubmodels";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/model-nodes") return "layout.getModelNodes";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/render-buffer-nodes") return "layout.getRenderBufferNodes";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/channel-map") return "layout.getChannelMap";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/scene") return "layout.getScene";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/settings") return "layout.getSettings";
        if (method == "GET" && path == "/xlightsdesigner/api/layout/group-members") return "layout.getGroupMembers";

        // Sequence elements expose row/order/selection state.
        if (method == "GET" && path == "/xlightsdesigner/api/elements/summary") return "elements.getSummary";
        if (method == "GET" && path == "/xlightsdesigner/api/elements/display-order") return "elements.getDisplayOrder";
        if (method == "POST" && path == "/xlightsdesigner/api/elements/display-order") return "elements.setDisplayOrder";
        if (method == "GET" && path == "/xlightsdesigner/api/elements/selected") return "elements.getSelected";
        if (method == "POST" && path == "/xlightsdesigner/api/elements/selected") return "elements.setSelected";

        return std::nullopt;
    }
};

} // namespace xLightsDesigner::api::transport
