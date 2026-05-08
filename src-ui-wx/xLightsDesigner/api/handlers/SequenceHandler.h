#pragma once

#include "../../DesignerApiRuntime.h"
#include "../models/SequenceModels.h"
#include "../services/SequenceService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"
#include "../validation/ValidationResult.h"

namespace xLightsDesigner::api::handlers {

class SequenceHandler {
public:
    explicit SequenceHandler(services::SequenceService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetOpen(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getOpenSequence();
        response.data["isOpen"] = summary.isOpen;
        response.data["sequence"] = summary.isOpen
            ? nlohmann::json{{"path", summary.path.value_or("")}, {"revisionToken", summary.revisionToken.value_or("")}}
            : nlohmann::json(nullptr);
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetRevision(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getRevision();
        if (!summary.isOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }

        response.data["sequencePath"] = summary.path.value_or("");
        response.data["revisionToken"] = summary.revisionToken.value_or("");
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetSettings(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto settings = _service.getSettings();
        if (!settings.isOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }

        response.data = serializeSettings(settings);
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleSetSettings(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::SequenceSettingsUpdateRequest updateRequest;
        if (auto it = request.params.find("sequenceType"); it != request.params.end()) {
            updateRequest.sequenceType = it->second;
        }
        if (auto it = request.params.find("durationMs"); it != request.params.end()) {
            updateRequest.durationMs = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("frameMs"); it != request.params.end()) {
            updateRequest.frameMs = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("supportsModelBlending"); it != request.params.end()) {
            updateRequest.supportsModelBlending = (it->second == "1" || it->second == "true" || it->second == "TRUE" || it->second == "yes" || it->second == "YES");
        }
        if (auto it = request.params.find("metadataAuthor"); it != request.params.end()) {
            updateRequest.metadataAuthor = it->second;
        }
        if (auto it = request.params.find("metadataAuthorEmail"); it != request.params.end()) {
            updateRequest.metadataAuthorEmail = it->second;
        }
        if (auto it = request.params.find("metadataWebsite"); it != request.params.end()) {
            updateRequest.metadataWebsite = it->second;
        }
        if (auto it = request.params.find("metadataSong"); it != request.params.end()) {
            updateRequest.metadataSong = it->second;
        }
        if (auto it = request.params.find("metadataArtist"); it != request.params.end()) {
            updateRequest.metadataArtist = it->second;
        }
        if (auto it = request.params.find("metadataAlbum"); it != request.params.end()) {
            updateRequest.metadataAlbum = it->second;
        }
        if (auto it = request.params.find("metadataMusicUrl"); it != request.params.end()) {
            updateRequest.metadataMusicUrl = it->second;
        }
        if (auto it = request.params.find("metadataComment"); it != request.params.end()) {
            updateRequest.metadataComment = it->second;
        }

        const auto validation = validateSetSettingsRequest(updateRequest);
        if (!validation.ok()) {
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& issue : validation.issues) {
                issues.push_back({{"field", issue.field}, {"code", issue.code}, {"message", issue.message}});
            }
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "Invalid sequence.setSettings request.",
                nlohmann::json{{"issues", issues}}
            };
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, updateRequest, this]() {
            transport::ApiResponse jobResponse;
            jobResponse.command = request.command;
            jobResponse.requestId = request.requestId;
            const auto result = service.updateSettings(updateRequest);
            if (!result.updated || !result.settings.isOpen) {
                int statusCode = 409;
                if (result.errorCode.has_value() && result.errorCode.value() == transport::errors::SequenceNotOpen) {
                    statusCode = 404;
                } else if (result.errorCode.has_value() && result.errorCode.value() == transport::errors::ValidationError) {
                    statusCode = 400;
                }
                jobResponse.statusCode = statusCode;
                jobResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to update the current sequence settings."),
                    nlohmann::json::object()
                };
                return jobResponse;
            }
            jobResponse.data["updated"] = true;
            jobResponse.data["settings"] = serializeSettings(result.settings);
            return jobResponse;
        });

        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleOpen(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::SequenceOpenRequest openRequest;
        auto it = request.params.find("file");
        if (it != request.params.end()) {
            openRequest.file = it->second;
        }
        it = request.params.find("force");
        if (it != request.params.end()) {
            openRequest.force = (it->second == "1" || it->second == "true" || it->second == "TRUE" || it->second == "yes" || it->second == "YES");
        }

        const auto validation = validateOpenRequest(openRequest);
        if (!validation.ok()) {
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& issue : validation.issues) {
                issues.push_back({{"field", issue.field}, {"code", issue.code}, {"message", issue.message}});
            }
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "Invalid sequence.open request.",
                nlohmann::json{{"issues", issues}}
            };
            return response;
        }

        if (!IsDesignerApiStartupSettled()) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                "APP_NOT_READY",
                "xLightsDesigner sequence.open is blocked until xLights startup has fully settled.",
                nlohmann::json{{"file", openRequest.file}, {"retryAfterMs", GetDesignerApiStartupSettleRemainingMs()}}
            };
            return response;
        }

        const auto current = _service.getOpenSequence();
        if (!openRequest.force && current.isOpen && current.path.has_value() && current.path.value() == openRequest.file) {
            response.data["opened"] = true;
            response.data["queued"] = false;
            response.data["sequence"] = nlohmann::json{{"path", current.path.value_or("")}, {"revisionToken", current.revisionToken.value_or("")}};
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, openRequest]() {
            transport::ApiResponse jobResponse;
            jobResponse.command = request.command;
            jobResponse.requestId = request.requestId;

            const auto result = service.openSequence(openRequest);
            if (!result.opened || !result.sequence.isOpen) {
                int statusCode = 404;
                if (result.errorCode.has_value() && result.errorCode.value() == "SEQUENCE_OPEN_TIMEOUT") {
                    statusCode = 504;
                } else if (result.errorCode.has_value() && result.errorCode.value() == "APP_NOT_READY") {
                    statusCode = 409;
                }
                jobResponse.statusCode = statusCode;
                nlohmann::json details{{"file", openRequest.file}};
                if (result.retryAfterMs.has_value()) {
                    details["retryAfterMs"] = *result.retryAfterMs;
                }
                jobResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::SequenceNotFound)),
                    result.errorMessage.value_or("Unable to open the requested sequence."),
                    details
                };
                return jobResponse;
            }

            jobResponse.data["opened"] = true;
            jobResponse.data["queued"] = false;
            jobResponse.data["sequence"] = nlohmann::json{{"path", result.sequence.path.value_or("")}, {"revisionToken", result.sequence.revisionToken.value_or("")}};
            return jobResponse;
        });

        response.statusCode = 202;
        response.data["opened"] = false;
        response.data["queued"] = true;
        response.data["requestedFile"] = openRequest.file;
        response.data["jobId"] = jobId;
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleCreate(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::SequenceCreateRequest createRequest;
        auto it = request.params.find("file");
        if (it != request.params.end()) {
            createRequest.file = it->second;
        }
        it = request.params.find("mediaFile");
        if (it != request.params.end()) {
            createRequest.mediaFile = it->second;
        }
        it = request.params.find("view");
        if (it != request.params.end()) {
            createRequest.view = it->second;
        }
        it = request.params.find("durationMs");
        if (it != request.params.end()) {
            createRequest.durationMs = std::atoi(it->second.c_str());
        }
        it = request.params.find("frameMs");
        if (it != request.params.end()) {
            createRequest.frameMs = std::atoi(it->second.c_str());
        }
        it = request.params.find("overwrite");
        if (it != request.params.end()) {
            createRequest.overwrite = (it->second == "1" || it->second == "true" || it->second == "TRUE" || it->second == "yes" || it->second == "YES");
        }

        const auto validation = validateCreateRequest(createRequest);
        if (!validation.ok()) {
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& issue : validation.issues) {
                issues.push_back({{"field", issue.field}, {"code", issue.code}, {"message", issue.message}});
            }
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "Invalid sequence.create request.",
                nlohmann::json{{"issues", issues}}
            };
            return response;
        }

        if (!IsDesignerApiStartupSettled()) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                "APP_NOT_READY",
                "xLightsDesigner sequence.create is blocked until xLights startup has fully settled.",
                nlohmann::json{{"file", createRequest.file}, {"retryAfterMs", GetDesignerApiStartupSettleRemainingMs()}}
            };
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, createRequest]() {
            transport::ApiResponse jobResponse;
            jobResponse.command = request.command;
            jobResponse.requestId = request.requestId;
            const auto result = service.createSequence(createRequest);
            if (!result.created || !result.sequence.isOpen) {
                int statusCode = 409;
                if (result.errorCode.has_value() && result.errorCode.value() == "VALIDATION_ERROR") {
                    statusCode = 400;
                } else if (result.errorCode.has_value() && result.errorCode.value() == "SEQUENCE_ACCESS_DENIED") {
                    statusCode = 403;
                }
                jobResponse.statusCode = statusCode;
                jobResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to create the requested sequence."),
                    nlohmann::json{{"file", createRequest.file}, {"mediaFile", createRequest.mediaFile}, {"durationMs", createRequest.durationMs}, {"frameMs", createRequest.frameMs}, {"overwrite", createRequest.overwrite}}
                };
                return jobResponse;
            }

            jobResponse.data["created"] = true;
            jobResponse.data["sequence"] = nlohmann::json{{"path", result.sequence.path.value_or("")}, {"revisionToken", result.sequence.revisionToken.value_or("")}};
            return jobResponse;
        });

        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleSave(const transport::ApiRequest& request) const {
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request]() {
            transport::ApiResponse response;
            response.command = request.command;
            response.requestId = request.requestId;
            const auto result = service.saveSequence();
            if (!result.saved || !result.sequence.isOpen) {
                response.statusCode = 409;
                response.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to save the current sequence."),
                    nlohmann::json{{"sequenceOpen", result.sequence.isOpen}, {"sequencePath", result.sequence.path.value_or("")}}
                };
                return response;
            }
            response.data["saved"] = true;
            response.data["sequence"] = nlohmann::json{{"path", result.sequence.path.value_or("")}, {"revisionToken", result.sequence.revisionToken.value_or("")}};
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleClose(const transport::ApiRequest& request) const {
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request]() {
            transport::ApiResponse response;
            response.command = request.command;
            response.requestId = request.requestId;
            const auto result = service.closeSequence();
            if (!result.closed) {
                response.statusCode = 409;
                response.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to close the current sequence."),
                    nlohmann::json::object()
                };
                return response;
            }
            response.data["closed"] = true;
            response.data["sequence"] = nlohmann::json{
                {"isOpen", result.sequence.isOpen},
                {"path", result.sequence.path.value_or("")},
                {"revisionToken", result.sequence.revisionToken.value_or("")}
            };
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleRenderCurrent(const transport::ApiRequest& request) const {
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request]() {
            transport::ApiResponse response;
            response.command = request.command;
            response.requestId = request.requestId;
            const auto result = service.renderSequence();
            if (!result.rendered || !result.sequence.isOpen) {
                response.statusCode = 409;
                response.error = transport::ApiError{
                    std::string(transport::errors::ValidationError),
                    "Unable to render the current sequence.",
                    nlohmann::json::object()
                };
                return response;
            }
            response.data["rendered"] = true;
            response.data["fseqPath"] = result.fseqPath.value_or("");
            response.data["sequence"] = nlohmann::json{{"path", result.sequence.path.value_or("")}, {"revisionToken", result.sequence.revisionToken.value_or("")}};
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleCheck(const transport::ApiRequest& request) const {
        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request]() {
            transport::ApiResponse response;
            response.command = request.command;
            response.requestId = request.requestId;
            const auto result = service.checkSequence();
            if (!result.checked) {
                response.statusCode = 409;
                response.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to check the current sequence."),
                    nlohmann::json::object()
                };
                return response;
            }

            nlohmann::json sections = nlohmann::json::array();
            for (const auto& section : result.sections) {
                nlohmann::json issues = nlohmann::json::array();
                for (const auto& issue : section.issues) {
                    issues.push_back({
                        {"type", issue.type},
                        {"message", issue.message},
                        {"category", issue.category},
                        {"modelName", issue.modelName},
                        {"effectName", issue.effectName},
                        {"startTimeMs", issue.startTimeMs},
                        {"layerIndex", issue.layerIndex}
                    });
                }
                sections.push_back({
                    {"id", section.id},
                    {"title", section.title},
                    {"description", section.description},
                    {"errorCount", section.errorCount},
                    {"warningCount", section.warningCount},
                    {"issues", issues}
                });
            }

            response.data["checked"] = true;
            response.data["sequenceOpen"] = result.sequenceOpen;
            response.data["errorCount"] = result.errorCount;
            response.data["warningCount"] = result.warningCount;
            response.data["showDirectory"] = result.showDirectory;
            response.data["sequencePath"] = result.sequencePath;
            response.data["generatedAt"] = result.generatedAt;
            response.data["sections"] = sections;
            response.data["sequence"] = nlohmann::json{
                {"isOpen", result.sequence.isOpen},
                {"path", result.sequence.path.value_or("")},
                {"revisionToken", result.sequence.revisionToken.value_or("")}
            };
            return response;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleExportPreviewVideo(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::SequencePreviewVideoExportRequest exportRequest;
        if (auto it = request.params.find("file"); it != request.params.end()) {
            exportRequest.file = it->second;
        }
        if (auto it = request.params.find("renderFirst"); it != request.params.end()) {
            exportRequest.renderFirst = (it->second == "1" || it->second == "true" || it->second == "TRUE" || it->second == "yes" || it->second == "YES");
        }
        exportRequest.width = parsing::ReadInt(request.params, "width", 0);
        exportRequest.height = parsing::ReadInt(request.params, "height", 0);

        const auto validation = validatePreviewVideoExportRequest(exportRequest);
        if (!validation.ok()) {
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& issue : validation.issues) {
                issues.push_back({{"field", issue.field}, {"code", issue.code}, {"message", issue.message}});
            }
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "Invalid sequence.exportPreviewVideo request.",
                nlohmann::json{{"issues", issues}}
            };
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, exportRequest]() {
            transport::ApiResponse jobResponse;
            jobResponse.command = request.command;
            jobResponse.requestId = request.requestId;
            const auto result = service.exportPreviewVideo(exportRequest);
            if (!result.exported || !result.sequence.isOpen) {
                int statusCode = 409;
                if (result.errorCode.has_value() && result.errorCode.value() == transport::errors::SequenceNotOpen) {
                    statusCode = 404;
                } else if (result.errorCode.has_value() && result.errorCode.value() == transport::errors::ValidationError) {
                    statusCode = 400;
                } else if (result.errorCode.has_value() && result.errorCode.value() == "SEQUENCE_ACCESS_DENIED") {
                    statusCode = 403;
                }
                jobResponse.statusCode = statusCode;
                jobResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("Unable to export the current sequence preview video."),
                    nlohmann::json{{"file", exportRequest.file}, {"renderFirst", exportRequest.renderFirst}, {"width", exportRequest.width}, {"height", exportRequest.height}}
                };
                return jobResponse;
            }
            jobResponse.data["exported"] = true;
            jobResponse.data["file"] = result.file.value_or(exportRequest.file);
            jobResponse.data["sequence"] = nlohmann::json{{"path", result.sequence.path.value_or("")}, {"revisionToken", result.sequence.revisionToken.value_or("")}};
            return jobResponse;
        });

        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleGetRenderSamples(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        models::SequenceRenderSamplesRequest sampleRequest;
        if (auto it = request.params.find("startMs"); it != request.params.end()) {
            sampleRequest.startMs = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("endMs"); it != request.params.end()) {
            sampleRequest.endMs = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("maxFrames"); it != request.params.end()) {
            sampleRequest.maxFrames = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("frameStride"); it != request.params.end()) {
            sampleRequest.frameStride = std::atoi(it->second.c_str());
        }
        if (auto it = request.params.find("channelRanges"); it != request.params.end()) {
            try {
                const auto rangesJson = nlohmann::json::parse(it->second);
                if (!rangesJson.is_array()) {
                    response.statusCode = 400;
                    response.error = transport::ApiError{
                        std::string(transport::errors::ValidationError),
                        "channelRanges must be a JSON array.",
                        nlohmann::json{{"param", "channelRanges"}}
                    };
                    return response;
                }
                for (const auto& rangeJson : rangesJson) {
                    if (!rangeJson.is_object()) {
                        continue;
                    }
                    models::SequenceChannelRangeRequest range;
                    range.startChannel = rangeJson.value("startChannel", 0);
                    range.channelCount = rangeJson.value("channelCount", 0);
                    sampleRequest.channelRanges.push_back(range);
                }
            } catch (const std::exception& ex) {
                response.statusCode = 400;
                response.error = transport::ApiError{
                    std::string(transport::errors::ValidationError),
                    "channelRanges must be valid JSON.",
                    nlohmann::json{{"param", "channelRanges"}, {"details", ex.what()}}
                };
                return response;
            }
        }

        if (sampleRequest.startMs < 0 || sampleRequest.endMs < sampleRequest.startMs) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "sequence.getRenderSamples requires a valid non-negative time range.",
                nlohmann::json{{"startMs", sampleRequest.startMs}, {"endMs", sampleRequest.endMs}}
            };
            return response;
        }
        if (sampleRequest.maxFrames <= 0 || sampleRequest.maxFrames > 32) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "maxFrames must be between 1 and 32.",
                nlohmann::json{{"maxFrames", sampleRequest.maxFrames}}
            };
            return response;
        }
        if (sampleRequest.frameStride < 0) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "frameStride must be zero or greater.",
                nlohmann::json{{"frameStride", sampleRequest.frameStride}}
            };
            return response;
        }
        if (sampleRequest.channelRanges.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{
                std::string(transport::errors::ValidationError),
                "channelRanges is required.",
                nlohmann::json{{"param", "channelRanges"}}
            };
            return response;
        }

        const auto result = _service.getRenderedSamples(sampleRequest);
        if (!result.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }
        if (!result.samplesAvailable) {
            response.statusCode = 409;
            response.error = transport::ApiError{
                "RENDER_SAMPLES_UNAVAILABLE",
                "Rendered samples are unavailable. Render the current sequence first.",
                nlohmann::json{{"startMs", sampleRequest.startMs}, {"endMs", sampleRequest.endMs}}
            };
            return response;
        }

        response.data["sequencePath"] = result.sequence.path.value_or("");
        response.data["revisionToken"] = result.sequence.revisionToken.value_or("");
        response.data["fseqPath"] = result.fseqPath.value_or("");
        response.data["frameMs"] = result.frameMs.value_or(0);
        response.data["totalFrames"] = result.totalFrames.value_or(0);
        response.data["totalChannels"] = result.totalChannels.value_or(0);
        response.data["startMs"] = result.startMs;
        response.data["endMs"] = result.endMs;
        response.data["sampledFrameCount"] = result.sampledFrameCount;
        response.data["sampledChannelCount"] = result.sampledChannelCount;
        response.data["sampleEncoding"] = result.sampleEncoding;
        response.data["channelRanges"] = nlohmann::json::array();
        for (const auto& range : result.channelRanges) {
            response.data["channelRanges"].push_back({
                {"startChannel", range.startChannel},
                {"channelCount", range.channelCount}
            });
        }
        response.data["samples"] = nlohmann::json::array();
        for (const auto& sample : result.samples) {
            response.data["samples"].push_back({
                {"frameIndex", sample.frameIndex},
                {"frameTimeMs", sample.frameTimeMs},
                {"dataBase64", sample.dataBase64}
            });
        }
        return response;
    }

    [[nodiscard]] validation::ValidationResult validateOpenRequest(const models::SequenceOpenRequest& request) const {
        validation::ValidationResult result;
        if (request.file.empty()) {
            result.addIssue("file", std::string(transport::errors::ValidationError), "file is required.");
        }
        return result;
    }

    [[nodiscard]] validation::ValidationResult validateCreateRequest(const models::SequenceCreateRequest& request) const {
        validation::ValidationResult result;
        if (request.file.empty()) {
            result.addIssue("file", std::string(transport::errors::ValidationError), "file is required.");
        }
        if (request.frameMs <= 0) {
            result.addIssue("frameMs", std::string(transport::errors::ValidationError), "frameMs must be greater than zero.");
        }
        if (request.mediaFile.empty() && request.durationMs <= 0) {
            result.addIssue("durationMs", std::string(transport::errors::ValidationError), "durationMs is required when mediaFile is not provided.");
        }
        return result;
    }

    [[nodiscard]] validation::ValidationResult validatePreviewVideoExportRequest(const models::SequencePreviewVideoExportRequest& request) const {
        validation::ValidationResult result;
        if (request.file.empty()) {
            result.addIssue("file", std::string(transport::errors::ValidationError), "file is required.");
        }
        if ((request.width < 0 || request.height < 0) || ((request.width > 0 || request.height > 0) && (request.width <= 0 || request.height <= 0))) {
            result.addIssue("width", std::string(transport::errors::ValidationError), "width and height must both be positive when either is provided.");
        }
        return result;
    }

    [[nodiscard]] validation::ValidationResult validateSetSettingsRequest(const models::SequenceSettingsUpdateRequest& request) const {
        validation::ValidationResult result;
        const bool hasAnyField = request.sequenceType.has_value() ||
            request.durationMs.has_value() ||
            request.frameMs.has_value() ||
            request.supportsModelBlending.has_value() ||
            request.metadataAuthor.has_value() ||
            request.metadataAuthorEmail.has_value() ||
            request.metadataWebsite.has_value() ||
            request.metadataSong.has_value() ||
            request.metadataArtist.has_value() ||
            request.metadataAlbum.has_value() ||
            request.metadataMusicUrl.has_value() ||
            request.metadataComment.has_value();
        if (!hasAnyField) {
            result.addIssue("request", std::string(transport::errors::ValidationError), "At least one sequence setting must be provided.");
        }
        if (request.durationMs.has_value() && request.durationMs.value() <= 0) {
            result.addIssue("durationMs", std::string(transport::errors::ValidationError), "durationMs must be greater than zero.");
        }
        if (request.frameMs.has_value() && request.frameMs.value() <= 0) {
            result.addIssue("frameMs", std::string(transport::errors::ValidationError), "frameMs must be greater than zero.");
        }
        return result;
    }

    [[nodiscard]] nlohmann::json serializeSettings(const models::SequenceSettings& settings) const {
        return nlohmann::json{
            {"sequencePath", settings.path.value_or("")},
            {"sequenceType", settings.sequenceType.value_or("")},
            {"mediaFile", settings.mediaFile.value_or("")},
            {"durationMs", settings.durationMs.value_or(0)},
            {"frameMs", settings.frameMs.value_or(0)},
            {"supportsModelBlending", settings.supportsModelBlending.value_or(false)},
            {"hasUnsavedChanges", settings.hasUnsavedChanges.value_or(false)}
        };
    }

private:
    services::SequenceService _service;
};

} // namespace xLightsDesigner::api::handlers
