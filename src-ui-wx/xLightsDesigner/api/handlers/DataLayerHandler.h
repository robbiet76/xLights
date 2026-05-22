#pragma once

#include <utility>

#include "../parsing/ParameterReaders.h"
#include "../services/DataLayerService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

// DataLayer endpoints are the bridge between direct-channel XLD output and
// xLights final rendering. They do not create native effect blocks.
class DataLayerHandler {
public:
    explicit DataLayerHandler(services::DataLayerService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleList(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        response.data = serializeList(_service.getDataLayers());
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleUpsert(const transport::ApiRequest& request) const {
        // Upsert requires enough FSEQ evidence for later sync-health checks to
        // detect stale or mismatched generated output.
        models::DataLayerUpsertRequest upsertRequest;
        upsertRequest.name = parsing::ReadString(request.params, "name");
        upsertRequest.sourceFseqPath = parsing::ReadString(request.params, "sourceFseqPath");
        upsertRequest.dataSourcePath = parsing::ReadString(request.params, "dataSourcePath", upsertRequest.sourceFseqPath);
        upsertRequest.placement = parsing::ReadString(request.params, "placement", "above-nutcracker");
        upsertRequest.channelCount = parsing::ReadInt(request.params, "channelCount", 0);
        upsertRequest.frameCount = parsing::ReadInt(request.params, "frameCount", 0);
        upsertRequest.frameMs = parsing::ReadInt(request.params, "frameMs", 0);
        upsertRequest.channelOffset = parsing::ReadInt(request.params, "channelOffset", 0);
        upsertRequest.expectedChecksum = parsing::ReadString(request.params, "expectedChecksum");
        upsertRequest.pathMode = parsing::ReadString(request.params, "pathMode", "relative-preferred");

        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        if (upsertRequest.name.empty() || upsertRequest.sourceFseqPath.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "sequence.data-layers.upsert requires name and sourceFseqPath.",
                nlohmann::json{{"name", upsertRequest.name}, {"sourceFseqPath", upsertRequest.sourceFseqPath}}
            };
            return response;
        }

        const auto result = _service.upsertDataLayer(upsertRequest);
        return serializeMutationResponse(request, result, "Unable to upsert DataLayer.");
    }

    [[nodiscard]] transport::ApiResponse handleRemove(const transport::ApiRequest& request) const {
        models::DataLayerRemoveRequest removeRequest;
        removeRequest.name = parsing::ReadString(request.params, "name");
        removeRequest.index = parsing::ReadInt(request.params, "index", -1);
        const auto result = _service.removeDataLayer(removeRequest);
        return serializeMutationResponse(request, result, "Unable to remove DataLayer.");
    }

    [[nodiscard]] transport::ApiResponse handleReorder(const transport::ApiRequest& request) const {
        models::DataLayerReorderRequest reorderRequest;
        reorderRequest.name = parsing::ReadString(request.params, "name");
        reorderRequest.index = parsing::ReadInt(request.params, "index", -1);
        reorderRequest.targetIndex = parsing::ReadInt(request.params, "targetIndex", -1);
        reorderRequest.placement = parsing::ReadString(request.params, "placement");
        const auto result = _service.reorderDataLayer(reorderRequest);
        return serializeMutationResponse(request, result, "Unable to reorder DataLayer.");
    }

    [[nodiscard]] transport::ApiResponse handleValidate(const transport::ApiRequest& request) const {
        models::DataLayerValidateRequest validateRequest;
        validateRequest.name = parsing::ReadString(request.params, "name");
        validateRequest.index = parsing::ReadInt(request.params, "index", -1);
        validateRequest.fseqPath = parsing::ReadString(request.params, "fseqPath");
        validateRequest.expectedChannelCount = parsing::ReadInt(request.params, "expectedChannelCount", 0);
        validateRequest.expectedFrameCount = parsing::ReadInt(request.params, "expectedFrameCount", 0);
        validateRequest.expectedFrameMs = parsing::ReadInt(request.params, "expectedFrameMs", 0);

        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        const auto result = _service.validateDataLayer(validateRequest);
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }
        if (!result.ok) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                result.errorCode.value_or("DATA_LAYER_VALIDATION_FAILED"),
                result.errorMessage.value_or("DataLayer validation failed."),
                serializeValidation(result)
            };
            return response;
        }
        response.data = serializeValidation(result);
        return response;
    }

private:
    [[nodiscard]] transport::ApiResponse serializeMutationResponse(const transport::ApiRequest& request,
                                                                   const models::DataLayerMutationResult& result,
                                                                   const std::string& fallbackMessage) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }
        if (!result.ok) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                result.errorMessage.value_or(fallbackMessage),
                nlohmann::json{{"sequencePath", result.sequencePath}}
            };
            return response;
        }
        response.data["ok"] = true;
        response.data["sequencePath"] = result.sequencePath;
        response.data["layer"] = serializeLayer(result.layer);
        response.data["layers"] = serializeList(result.layers);
        return response;
    }

    static nlohmann::json serializeFseq(const models::FseqFileSummary& fseq) {
        return {
            {"exists", fseq.exists},
            {"readable", fseq.readable},
            {"path", fseq.path},
            {"versionMajor", fseq.versionMajor},
            {"versionMinor", fseq.versionMinor},
            {"frameMs", fseq.frameMs},
            {"frameCount", fseq.frameCount},
            {"channelCount", fseq.channelCount},
            {"maxChannel", fseq.maxChannel},
            {"modifiedAt", fseq.modifiedAt}
        };
    }

    static nlohmann::json serializeXldManifest(const models::XldManifestValidationSummary& manifest) {
        return {
            {"expected", manifest.expected},
            {"exists", manifest.exists},
            {"readable", manifest.readable},
            {"path", manifest.path},
            {"status", manifest.status},
            {"issueCodes", manifest.issueCodes},
            {"warnings", manifest.warnings},
            {"artifactType", manifest.artifactType},
            {"artifactVersion", manifest.artifactVersion},
            {"appId", manifest.appId},
            {"targetSequencePath", manifest.targetSequencePath},
            {"generatedFseqPath", manifest.generatedFseqPath},
            {"generatedFseqBasename", manifest.generatedFseqBasename},
            {"generatedFseqSizeBytes", manifest.generatedFseqSizeBytes},
            {"displaySnapshotId", manifest.displaySnapshotId},
            {"channelMapSnapshotId", manifest.channelMapSnapshotId},
            {"channelMapFingerprint", manifest.channelMapFingerprint},
            {"outputConfigurationFingerprint", manifest.outputConfigurationFingerprint},
            {"frameMs", manifest.frameMs},
            {"frameCount", manifest.frameCount},
            {"channelCount", manifest.channelCount},
            {"dataLayerName", manifest.dataLayerName},
            {"generatedAt", manifest.generatedAt}
        };
    }

    static nlohmann::json serializeLayer(const models::DataLayerSummary& layer) {
        nlohmann::json value{
            {"index", layer.index},
            {"name", layer.name},
            {"source", layer.source},
            {"dataSource", layer.dataSource},
            {"placement", layer.placement},
            {"pathMode", layer.pathMode},
            {"isNutcracker", layer.isNutcracker},
            {"isXldLayer", layer.isXldLayer},
            {"pathExists", layer.pathExists},
            {"participatesInFinalRender", layer.participatesInFinalRender},
            {"numChannels", layer.numChannels},
            {"numFrames", layer.numFrames},
            {"channelOffset", layer.channelOffset},
            {"lorConvertParams", layer.lorConvertParams}
        };
        if (layer.fseq.has_value()) {
            value["fseq"] = serializeFseq(*layer.fseq);
        }
        if (layer.xldManifest.has_value()) {
            value["xldManifest"] = serializeXldManifest(*layer.xldManifest);
        }
        return value;
    }

    static nlohmann::json serializeList(const models::DataLayerListSummary& summary) {
        nlohmann::json value{
            {"sequenceOpen", summary.sequenceOpen},
            {"sequencePath", summary.sequencePath},
            {"revisionToken", summary.revisionToken},
            {"layers", nlohmann::json::array()}
        };
        for (const auto& layer : summary.layers) {
            value["layers"].push_back(serializeLayer(layer));
        }
        return value;
    }

    static nlohmann::json serializeValidation(const models::DataLayerValidationResult& result) {
        return {
            {"ok", result.ok},
            {"sequenceOpen", result.sequenceOpen},
            {"fileExists", result.fileExists},
            {"readable", result.readable},
            {"supportedVersion", result.supportedVersion},
            {"path", result.path},
            {"layer", serializeLayer(result.layer)},
            {"fseq", serializeFseq(result.fseq)},
            {"warnings", result.warnings}
        };
    }

    services::DataLayerService _service;
};

} // namespace xLightsDesigner::api::handlers
