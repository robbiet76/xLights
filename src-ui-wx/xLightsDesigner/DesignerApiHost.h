#pragma once

#include <future>
#include <wx/filename.h>
#include <wx/base64.h>
#include <memory>
#include <cmath>
#include <algorithm>

#include "FSEQFile.h"
#include "SequenceFile.h"
#include "xLightsMain.h"
#include "models/DisplayAsType.h"
#include "models/ModelGroup.h"
#include "ExternalHooks.h"
#include "DesignerApiRuntime.h"
#include "api/models/EffectModels.h"
#include "api/models/ElementModels.h"
#include "api/models/LayoutModels.h"
#include "api/models/MediaModels.h"
#include "api/models/SequenceModels.h"
#include "api/models/TimingModels.h"

namespace xLightsDesigner {

namespace detail {
inline std::string FormatDesignerApiDateTime(const wxDateTime& value) {
    if (!value.IsValid()) {
        return std::string();
    }
    return value.ToUTC().FormatISOCombined('T').ToStdString() + "Z";
}

inline std::string ReadDesignerApiFileModifiedAt(const wxFileName& fileName) {
    if (!fileName.FileExists()) {
        return std::string();
    }
    return FormatDesignerApiDateTime(fileName.GetModificationTime());
}

inline std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string BuildDesignerSequenceRevisionToken(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return std::string();
    }
    const std::string path = frame->CurrentSeqXmlFile->GetFullPath();
    return path.empty() ? std::string() : path + "#" + std::to_string(frame->GetSequenceElements().GetChangeCount());
}

inline std::string ResolveDesignerRenderedFseqPath(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return std::string();
    }
    const auto xsqPath = frame->CurrentSeqXmlFile->GetFullPath();
    if (!xsqPath.empty()) {
        const auto resolved = SequenceFile::GetFSEQForXSQ(xsqPath, frame->GetFseqDirectory());
        if (!resolved.empty()) {
            return resolved;
        }
    }
    const auto currentFilename = xLightsFrame::GetFilename();
    if (!currentFilename.empty() && wxFileExists(currentFilename)) {
        return currentFilename;
    }
    return std::string();
}

inline wxString BuildDesignerRenderedFseqPath(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return wxString();
    }

    wxFileName output(frame->CurrentSeqXmlFile->GetFullPath());
    output.SetExt("fseq");
    if (!frame->GetFseqDirectory().empty()) {
        output.SetPath(frame->GetFseqDirectory());
    }
    return output.GetFullPath();
}

inline wxString FindDesignerShowDirectoryForSequence(const std::string& sequenceFile) {
    wxString showDir = wxPathOnly(wxString::FromUTF8(sequenceFile));
    while (!showDir.empty()) {
        if (FileExists(showDir + "/" + XLIGHTS_RGBEFFECTS_FILE) || FileExists(showDir + "/" + XLIGHTS_NETWORK_FILE)) {
            return showDir;
        }
        const wxString parent = wxPathOnly(showDir);
        if (parent == showDir) {
            break;
        }
        showDir = parent;
    }
    return wxString();
}

inline bool PrepareDesignerShowDirectoryForSequence(xLightsFrame* frame, const std::string& sequenceFile) {
    if (frame == nullptr || sequenceFile.empty()) {
        return false;
    }
    const wxString targetShowDir = FindDesignerShowDirectoryForSequence(sequenceFile);
    if (targetShowDir.empty()) {
        return true;
    }
    if (frame->CurrentDir == targetShowDir) {
        return true;
    }
    return frame->SetDir(targetShowDir, true);
}
}

class DesignerApiHost {
public:
    explicit DesignerApiHost(xLightsFrame* frame)
        : _frame(frame) {}

    [[nodiscard]] api::models::SequenceSummary readOpenSequence() const {
        api::models::SequenceSummary summary;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.isOpen = true;
        summary.path = _frame->CurrentSeqXmlFile->GetFullPath();
        summary.revisionToken = detail::BuildDesignerSequenceRevisionToken(_frame);
        return summary;
    }

    [[nodiscard]] api::models::SequenceSettings readSequenceSettings() const {
        api::models::SequenceSettings settings;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return settings;
        }

        settings.isOpen = true;
        settings.path = _frame->CurrentSeqXmlFile->GetFullPath();
        settings.sequenceType = _frame->CurrentSeqXmlFile->GetSequenceType();
        settings.mediaFile = _frame->CurrentSeqXmlFile->GetMediaFile();
        settings.durationMs = _frame->CurrentSeqXmlFile->GetSequenceDurationMS();
        settings.frameMs = _frame->CurrentSeqXmlFile->GetFrameMS();
        settings.hasUnsavedChanges = (_frame->mSavedChangeCount != _frame->GetSequenceElements().GetChangeCount());
        return settings;
    }

    [[nodiscard]] api::models::LayoutSettingsSummary readLayoutSettings() const {
        api::models::LayoutSettingsSummary settings;
        if (_frame == nullptr) {
            return settings;
        }

        settings.hasUnsavedRgbEffectsChanges = _frame->UnsavedRgbEffectsChanges;
        settings.hasUnsavedNetworkChanges = _frame->UnsavedNetworkChanges;
        settings.hasUnsavedLayoutChanges = settings.hasUnsavedRgbEffectsChanges || settings.hasUnsavedNetworkChanges;
        settings.modelsChangeCount = _frame->modelsChangeCount;
        settings.showDirectory = _frame->CurrentDir.ToStdString();

        const wxFileName rgbEffectsFile(_frame->CurrentDir, XLIGHTS_RGBEFFECTS_FILE);
        settings.rgbEffectsFile = rgbEffectsFile.GetFullPath().ToStdString();
        settings.rgbEffectsModifiedAt = detail::ReadDesignerApiFileModifiedAt(rgbEffectsFile);

        const wxFileName networksFile(_frame->CurrentDir, OutputManager::GetNetworksFileName());
        settings.networksFile = networksFile.GetFullPath().ToStdString();
        settings.networksModifiedAt = detail::ReadDesignerApiFileModifiedAt(networksFile);
        return settings;
    }


    [[nodiscard]] api::models::SequenceOpenResult openSequence(const api::models::SequenceOpenRequest& request) const {
        api::models::SequenceOpenResult result;
        result.requestedPath = request.file;
        if (_frame == nullptr || request.file.empty()) {
            return result;
        }

        if (!IsDesignerApiStartupSettled()) {
            result.errorCode = "APP_NOT_READY";
            result.errorMessage = "xLightsDesigner sequence.open is blocked until xLights startup has fully settled.";
            result.retryAfterMs = GetDesignerApiStartupSettleRemainingMs();
            return result;
        }

        const auto current = readOpenSequence();
        if (!request.force && current.isOpen && current.path.has_value() && current.path.value() == request.file) {
            result.opened = true;
            result.sequence = current;
            return result;
        }

        if (wxIsMainThread()) {
            if (!ObtainAccessToURL(request.file, false)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain access to the requested sequence file.";
                return result;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(_frame, request.file)) {
                result.errorCode = "SHOW_DIRECTORY_FAILED";
                result.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                return result;
            }
            _frame->OpenSequence(wxString::FromUTF8(request.file), nullptr);
            const auto opened = readOpenSequence();
            if (opened.isOpen && opened.path.has_value() && opened.path.value() == request.file) {
                result.opened = true;
                result.sequence = opened;
                return result;
            }
            result.errorCode = "SEQUENCE_OPEN_FAILED";
            result.errorMessage = "xLights did not report the requested sequence as open after OpenSequence completed.";
            return result;
        }

        auto promise = std::make_shared<std::promise<api::models::SequenceOpenResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        const std::string requestedFile = request.file;
        frame->CallAfter([this, promise, requestedFile]() mutable {
            api::models::SequenceOpenResult callbackResult;
            callbackResult.requestedPath = requestedFile;
            if (!ObtainAccessToURL(requestedFile, false)) {
                callbackResult.errorCode = "SEQUENCE_ACCESS_DENIED";
                callbackResult.errorMessage = "Unable to obtain access to the requested sequence file.";
                promise->set_value(callbackResult);
                return;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(_frame, requestedFile)) {
                callbackResult.errorCode = "SHOW_DIRECTORY_FAILED";
                callbackResult.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                promise->set_value(callbackResult);
                return;
            }
            _frame->OpenSequence(wxString::FromUTF8(requestedFile), nullptr);
            const auto opened = readOpenSequence();
            if (opened.isOpen && opened.path.has_value() && opened.path.value() == requestedFile) {
                callbackResult.opened = true;
                callbackResult.sequence = opened;
            } else {
                callbackResult.errorCode = "SEQUENCE_OPEN_FAILED";
                callbackResult.errorMessage = "xLights did not report the requested sequence as open after OpenSequence completed.";
            }
            promise->set_value(callbackResult);
        });
        if (future.wait_for(std::chrono::seconds(90)) != std::future_status::ready) {
            result.errorCode = "SEQUENCE_OPEN_TIMEOUT";
            result.errorMessage = "Timed out waiting for xLights to finish opening the requested sequence.";
            return result;
        }
        return future.get();
    }

    [[nodiscard]] api::models::SequenceCreateResult createSequence(const api::models::SequenceCreateRequest& request) const {
        return RunOnMainThread<api::models::SequenceCreateResult>([this, request]() {
            api::models::SequenceCreateResult result;
            result.requestedPath = request.file;
            if (_frame == nullptr || request.file.empty()) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "sequence.create requires a target file path.";
                return result;
            }
            if (_frame->IsReadOnlyMode()) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "Sequences cannot be created in read only mode.";
                return result;
            }

            wxFileName targetFile(wxString::FromUTF8(request.file));
            targetFile.SetExt("xsq");
            if (!targetFile.DirExists()) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "Target sequence directory does not exist.";
                return result;
            }
            if (targetFile.FileExists() && !request.overwrite) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "Target sequence file already exists and overwrite was not enabled.";
                return result;
            }
            const std::string targetAccessPath = targetFile.FileExists()
                ? targetFile.GetFullPath().ToStdString()
                : targetFile.GetPath().ToStdString();
            if (!ObtainAccessToURL(targetAccessPath, true)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain write access to the requested sequence path.";
                return result;
            }
            if (!request.mediaFile.empty() && !ObtainAccessToURL(request.mediaFile, false)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain access to the requested media file.";
                return result;
            }

            const std::string mediaFile = request.mediaFile == "null" ? std::string() : request.mediaFile;
            const std::string view = request.view == "null" ? std::string() : request.view;
            _frame->NewSequence(mediaFile, static_cast<uint32_t>(request.durationMs > 0 ? request.durationMs : 0), static_cast<uint32_t>(request.frameMs > 0 ? request.frameMs : 25), view);
            _frame->EnableSequenceControls(true);
            if (_frame->CurrentSeqXmlFile == nullptr) {
                result.errorCode = "CREATE_FAILED";
                result.errorMessage = "Failed to create the requested sequence.";
                return result;
            }

            _frame->SaveAsSequence(targetFile.GetFullPath().ToStdString());
            const auto created = readOpenSequence();
            if (!created.isOpen || !created.path.has_value() || created.path.value() != targetFile.GetFullPath().ToStdString()) {
                result.errorCode = "CREATE_FAILED";
                result.errorMessage = "xLights did not report the requested sequence as open after creation.";
                return result;
            }
            result.created = true;
            result.sequence = created;
            return result;
        });
    }

    [[nodiscard]] api::models::SequenceCloseResult closeSequence() const {
        return RunOnMainThread<api::models::SequenceCloseResult>([this]() {
            api::models::SequenceCloseResult result;
            if (_frame == nullptr) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "xLights frame is unavailable.";
                return result;
            }

            if (_frame->CurrentSeqXmlFile != nullptr &&
                _frame->mSavedChangeCount != _frame->GetSequenceElements().GetChangeCount()) {
                _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
                _frame->mLastAutosaveCount = _frame->mSavedChangeCount;
            }
            const bool closed = _frame->CloseSequence();
            if (!closed) {
                result.errorCode = "SEQUENCE_CLOSE_FAILED";
                result.errorMessage = "Unable to close the current sequence.";
                result.sequence = readOpenSequence();
                return result;
            }
            result.closed = true;
            result.sequence = readOpenSequence();
            return result;
        });
    }

    [[nodiscard]] api::models::SequenceSaveResult saveSequence() const {
        api::models::SequenceSaveResult result;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return result;
        }

        auto finalizeResult = [this]() {
            api::models::SequenceSaveResult saveResult;
            saveResult.sequence = readOpenSequence();
            saveResult.saved = saveResult.sequence.isOpen;
            return saveResult;
        };

        if (_frame->CurrentSeqXmlFile->GetFullPath().empty() || _frame->IsReadOnlyMode()) {
            return result;
        }

        if (_frame->mSavedChangeCount == _frame->GetSequenceElements().GetChangeCount()) {
            return finalizeResult();
        }

        if (!ObtainAccessToURL(_frame->CurrentSeqXmlFile->GetFullPath(), true)) {
            return result;
        }

        auto performSave = [this]() {
            wxCommandEvent playEvent(EVT_STOP_SEQUENCE);
            wxPostEvent(_frame, playEvent);
            std::unique_lock<std::mutex> lock(_frame->saveLock);
            const auto ext = detail::ToLowerCopy(_frame->CurrentSeqXmlFile->GetExt());
            if (ext == "xml") {
                wxRemoveFile(_frame->CurrentSeqXmlFile->GetFullPath());
                _frame->CurrentSeqXmlFile->SetExt("xsq");
            } else if (ext == "xbkp") {
                _frame->CurrentSeqXmlFile->SetExt("xsq");
            }
            const bool ok = _frame->CurrentSeqXmlFile->Save(_frame->GetSequenceElements());
            if (ok) {
                _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
                _frame->mLastAutosaveCount = _frame->mSavedChangeCount;
            }
            return ok;
        };

        if (wxIsMainThread()) {
            if (!performSave()) {
                return result;
            }
            return finalizeResult();
        }

        auto promise = std::make_shared<std::promise<api::models::SequenceSaveResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([this, promise, performSave, finalizeResult]() mutable {
            if (!performSave()) {
                promise->set_value(api::models::SequenceSaveResult{});
                return;
            }
            promise->set_value(finalizeResult());
        });
        return future.get();
    }

    [[nodiscard]] api::models::SequenceRenderResult renderCurrentSequence() const {
        api::models::SequenceRenderResult result;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return result;
        }

        auto finalizeResult = [this]() {
            api::models::SequenceRenderResult renderResult;
            renderResult.sequence = readOpenSequence();
            renderResult.rendered = renderResult.sequence.isOpen;
            return renderResult;
        };

        auto performRender = [this]() {
            _frame->RenderAll();
            if (_frame->CurrentSeqXmlFile == nullptr) {
                return false;
            }

            const wxString renderedFseqPath = detail::BuildDesignerRenderedFseqPath(_frame);
            if (renderedFseqPath.empty()) {
                return false;
            }
            ObtainAccessToURL(renderedFseqPath.ToStdString());
            xLightsFrame::xlightsFilename = renderedFseqPath;
            _frame->WriteFalconPiFile(renderedFseqPath);
            return _frame->CurrentSeqXmlFile != nullptr;
        };

        if (wxIsMainThread()) {
            if (!performRender()) {
                return result;
            }
            return finalizeResult();
        }

        auto promise = std::make_shared<std::promise<api::models::SequenceRenderResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([promise, performRender, finalizeResult]() mutable {
            if (!performRender()) {
                promise->set_value(api::models::SequenceRenderResult{});
                return;
            }
            promise->set_value(finalizeResult());
        });
        return future.get();
    }

    [[nodiscard]] api::models::SequenceRenderSamplesResult readRenderedSamples(const api::models::SequenceRenderSamplesRequest& request) const {
        api::models::SequenceRenderSamplesResult result;
        result.sequence = readOpenSequence();
        result.sequenceOpen = result.sequence.isOpen;
        result.startMs = request.startMs;
        result.endMs = request.endMs;
        result.channelRanges = request.channelRanges;

        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr || !result.sequence.isOpen) {
            return result;
        }

        const auto fseqPath = detail::ResolveDesignerRenderedFseqPath(_frame);
        if (fseqPath.empty()) {
            return result;
        }
        result.fseqPath = fseqPath;

        std::unique_ptr<FSEQFile> file(FSEQFile::openFSEQFile(fseqPath));
        if (!file) {
            return result;
        }

        const int frameMs = static_cast<int>(file->getStepTime());
        const int totalFrames = static_cast<int>(file->getNumFrames());
        const int totalChannels = static_cast<int>(file->getChannelCount());
        result.frameMs = frameMs;
        result.totalFrames = totalFrames;
        result.totalChannels = totalChannels;

        if (frameMs <= 0 || totalFrames <= 0 || totalChannels <= 0 || request.channelRanges.empty()) {
            return result;
        }

        const int normalizedStartMs = std::max(0, request.startMs);
        const int normalizedEndMs = std::max(normalizedStartMs, request.endMs);
        result.startMs = normalizedStartMs;
        result.endMs = normalizedEndMs;

        std::vector<std::pair<uint32_t, uint32_t>> readRanges;
        readRanges.reserve(request.channelRanges.size());
        int packedChannelCount = 0;
        for (const auto& range : request.channelRanges) {
            if (range.startChannel <= 0 || range.channelCount <= 0) {
                continue;
            }
            const int startChannelZero = range.startChannel - 1;
            if (startChannelZero >= totalChannels) {
                continue;
            }
            const int boundedCount = std::min(range.channelCount, totalChannels - startChannelZero);
            if (boundedCount <= 0) {
                continue;
            }
            readRanges.emplace_back(static_cast<uint32_t>(startChannelZero), static_cast<uint32_t>(boundedCount));
            packedChannelCount += boundedCount;
        }
        result.sampledChannelCount = packedChannelCount;
        if (readRanges.empty() || packedChannelCount <= 0) {
            return result;
        }

        const int startFrame = std::min(totalFrames - 1, normalizedStartMs / frameMs);
        const int endFrame = std::min(totalFrames - 1, normalizedEndMs / frameMs);
        if (startFrame < 0 || endFrame < startFrame) {
            return result;
        }

        std::vector<int> frameIndexes;
        if (request.frameStride > 0) {
            frameIndexes.reserve(std::max(1, request.maxFrames));
            for (int frameIndex = startFrame;
                 frameIndex <= endFrame && static_cast<int>(frameIndexes.size()) < std::max(1, request.maxFrames);
                 frameIndex += request.frameStride) {
                frameIndexes.push_back(frameIndex);
            }
            if (frameIndexes.empty()) {
                frameIndexes.push_back(startFrame);
            }
        } else {
            const int targetFrames = std::max(1, request.maxFrames);
            frameIndexes.reserve(targetFrames);
            if (targetFrames == 1 || startFrame == endFrame) {
                frameIndexes.push_back(startFrame);
            } else {
                const double span = static_cast<double>(endFrame - startFrame);
                for (int sampleIndex = 0; sampleIndex < targetFrames; sampleIndex++) {
                    const double ratio = static_cast<double>(sampleIndex) / static_cast<double>(targetFrames - 1);
                    const int frameIndex = startFrame + static_cast<int>(std::llround(span * ratio));
                    if (frameIndexes.empty() || frameIndexes.back() != frameIndex) {
                        frameIndexes.push_back(frameIndex);
                    }
                }
            }
        }

        file->prepareRead(readRanges, static_cast<uint32_t>(startFrame));
        std::vector<uint8_t> fullFrame(static_cast<size_t>(totalChannels), 0);
        for (const int frameIndex : frameIndexes) {
            std::unique_ptr<FSEQFile::FrameData> frame(file->getFrame(static_cast<uint32_t>(frameIndex)));
            if (!frame) {
                continue;
            }
            std::fill(fullFrame.begin(), fullFrame.end(), 0);
            if (!frame->readFrame(fullFrame.data(), static_cast<uint32_t>(totalChannels))) {
                continue;
            }

            std::vector<uint8_t> packedFrame(static_cast<size_t>(packedChannelCount), 0);
            size_t packedOffset = 0;
            for (const auto& range : readRanges) {
                const size_t count = static_cast<size_t>(range.second);
                memcpy(&packedFrame[packedOffset], &fullFrame[range.first], count);
                packedOffset += count;
            }

            api::models::SequenceRenderedFrameSample sample;
            sample.frameIndex = frameIndex;
            sample.frameTimeMs = frameIndex * frameMs;
            sample.dataBase64 = wxBase64Encode(packedFrame.data(), packedFrame.size()).ToStdString();
            result.samples.push_back(std::move(sample));
        }

        result.sampledFrameCount = static_cast<int>(result.samples.size());
        result.samplesAvailable = !result.samples.empty();
        return result;
    }

    [[nodiscard]] api::models::MediaSummary readCurrentMedia() const {
        api::models::MediaSummary summary;
        if (_frame == nullptr) {
            return summary;
        }

        summary.showDirectory = _frame->GetShowDirectory();
        if (_frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        summary.sequencePath = _frame->CurrentSeqXmlFile->GetFullPath();
        summary.mediaFile = _frame->CurrentSeqXmlFile->GetMediaFile();
        return summary;
    }

    [[nodiscard]] api::models::MediaDirectoriesSummary readMediaDirectories() const {
        api::models::MediaDirectoriesSummary summary;
        if (_frame == nullptr) {
            return summary;
        }

        for (const auto& directory : _frame->GetMediaFolders()) {
            summary.directories.push_back(directory);
        }
        return summary;
    }

    [[nodiscard]] api::models::LayoutModelsSummary readLayoutModels() const {
        api::models::LayoutModelsSummary summary;
        if (_frame == nullptr) {
            return summary;
        }

        summary.models.reserve(_frame->AllModels.size());
        for (auto it = _frame->AllModels.begin(); it != _frame->AllModels.end(); ++it) {
            const auto* model = it->second;
            if (model == nullptr) {
                continue;
            }
            const auto& location = model->GetModelScreenLocation();
            const auto position = location.GetWorldPosition();
            api::models::LayoutModelSummary modelSummary;
            modelSummary.name = model->GetName();
            modelSummary.displayAs = DisplayAsTypeToString(model->GetDisplayAs());
            modelSummary.stringType = model->GetStringType();
            modelSummary.layoutGroup = model->GetLayoutGroup();
            modelSummary.startChannel = static_cast<int>(model->GetFirstChannel()) + 1;
            modelSummary.endChannel = static_cast<int>(model->GetLastChannel()) + 1;
            modelSummary.submodelCount = static_cast<int>(model->GetSubModels().size());
            modelSummary.nodeCount = static_cast<int>(model->GetNodeCount());
            modelSummary.positionX = static_cast<double>(position.x);
            modelSummary.positionY = static_cast<double>(position.y);
            modelSummary.positionZ = static_cast<double>(position.z);
            modelSummary.width = static_cast<double>(location.GetMWidth());
            modelSummary.height = static_cast<double>(location.GetMHeight());
            modelSummary.depth = static_cast<double>(location.GetMDepth());
            summary.models.push_back(std::move(modelSummary));
        }
        return summary;
    }

    [[nodiscard]] api::models::LayoutGroupMembershipsSummary readLayoutGroupMemberships() const {
        api::models::LayoutGroupMembershipsSummary summary;
        if (_frame == nullptr) {
            return summary;
        }

        for (auto it = _frame->AllModels.begin(); it != _frame->AllModels.end(); ++it) {
            auto* model = it->second;
            if (model == nullptr || model->GetDisplayAs() != DisplayAsType::ModelGroup) {
                continue;
            }
            auto* group = dynamic_cast<ModelGroup*>(model);
            if (group == nullptr) {
                continue;
            }

            api::models::LayoutGroupMembersSummary groupSummary;
            groupSummary.groupName = model->GetName();

            for (const auto& name : group->ModelNames()) {
                Model* member = _frame->AllModels.GetModel(name);
                if (member == nullptr) {
                    continue;
                }
                groupSummary.directMembers.push_back(makeLayoutGroupMemberSummary(member));
            }

            for (const auto& member : group->ActiveModels()) {
                if (member == nullptr) {
                    continue;
                }
                groupSummary.activeMembers.push_back(makeLayoutGroupMemberSummary(member));
            }

            for (const auto& member : group->GetFlatModels(true, true)) {
                if (member == nullptr) {
                    continue;
                }
                groupSummary.flattenedMembers.push_back(makeLayoutGroupMemberSummary(member));
            }

            for (const auto& member : group->GetFlatModels(true, false)) {
                if (member == nullptr) {
                    continue;
                }
                groupSummary.flattenedAllMembers.push_back(makeLayoutGroupMemberSummary(member));
            }

            summary.groups.push_back(std::move(groupSummary));
        }
        return summary;
    }

    [[nodiscard]] api::models::ElementsSummary readElements() const {
        api::models::ElementsSummary summary;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        auto& sequenceElements = _frame->GetSequenceElements();
        summary.elements.reserve(sequenceElements.GetElementCount());
        for (size_t i = 0; i < sequenceElements.GetElementCount(); ++i) {
            Element* element = sequenceElements.GetElement(i);
            if (element == nullptr) {
                continue;
            }

            api::models::SequenceElementSummary elementSummary;
            elementSummary.name = element->GetName();
            elementSummary.type = element->GetTypeDescription();
            elementSummary.totalEffectCount = element->GetEffectCount();
            elementSummary.layers.reserve(element->GetEffectLayerCount());

            for (size_t layerIndex = 0; layerIndex < element->GetEffectLayerCount(); ++layerIndex) {
                EffectLayer* layer = element->GetEffectLayer(static_cast<int>(layerIndex));
                if (layer == nullptr) {
                    continue;
                }
                elementSummary.layers.push_back({
                    layer->GetLayerNumber(),
                    layer->GetEffectCount(),
                    layer->GetLayerName()
                });
            }

            summary.elements.push_back(std::move(elementSummary));
        }
        return summary;
    }

    [[nodiscard]] api::models::ClearEffectWindowResult clearEffectsWindow(const api::models::ClearEffectWindowRequest& request) const {
        return RunOnMainThread<api::models::ClearEffectWindowResult>([this, request]() {
            api::models::ClearEffectWindowResult result;
            result.elementName = request.elementName;
            result.layerNumber = request.layerNumber;
            result.startMs = request.startMs;
            result.endMs = request.endMs;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;
            if (static_cast<int>(element->GetEffectLayerCount()) <= request.layerNumber) return result;
            EffectLayer* layer = element->GetEffectLayer(request.layerNumber);
            if (layer == nullptr) return result;
            result.layerFound = true;
            std::vector<int> indexesToDelete;
            for (int i = 0; i < layer->GetEffectCount(); ++i) {
                Effect* effect = layer->GetEffect(i);
                if (effect == nullptr) continue;
                if (effect->GetEndTimeMS() < request.startMs || effect->GetStartTimeMS() > request.endMs) continue;
                indexesToDelete.push_back(i);
            }
            for (auto it = indexesToDelete.rbegin(); it != indexesToDelete.rend(); ++it) {
                layer->DeleteEffectByIndex(*it);
                result.clearedEffectCount++;
            }
            if (result.clearedEffectCount > 0) {
                _frame->MarkEffectsFileDirty();
                wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
                wxPostEvent(_frame, eventRowHeaderChanged);
            }
            return result;
        });
    }

    [[nodiscard]] api::models::AddEffectResult addEffect(const api::models::AddEffectRequest& request) const {
        return RunOnMainThread<api::models::AddEffectResult>([this, request]() {
            api::models::AddEffectResult result;
            result.elementName = request.elementName;
            result.layerNumber = request.layerNumber;
            result.effectName = request.effectName;
            result.startMs = request.startMs;
            result.endMs = request.endMs;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;
            while (static_cast<int>(element->GetEffectLayerCount()) <= request.layerNumber) {
                if (element->AddEffectLayer() == nullptr) return result;
            }
            EffectLayer* layer = element->GetEffectLayer(request.layerNumber);
            if (layer == nullptr) return result;
            Effect* effect = layer->AddEffect(0, request.effectName, request.settings, request.palette, request.startMs, request.endMs, EFFECT_NOT_SELECTED, false);
            if (effect == nullptr) return result;
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.created = true;
            return result;
        });
    }

    [[nodiscard]] api::models::ApplyEffectBatchResult applyEffectBatch(const api::models::ApplyEffectBatchRequest& request) const {
        return RunOnMainThread<api::models::ApplyEffectBatchResult>([this, request]() {
            api::models::ApplyEffectBatchResult result;
            result.requestedCount = static_cast<int>(request.effects.size());
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;

            std::vector<Element*> elements;
            elements.reserve(request.effects.size());
            for (std::size_t i = 0; i < request.effects.size(); ++i) {
                const auto& item = request.effects[i];
                Element* element = _frame->GetSequenceElements().GetElement(item.elementName);
                if (element == nullptr) {
                    result.failedItemIndex = static_cast<int>(i);
                    result.errorCode = std::string("VALIDATION_ERROR");
                    result.errorMessage = std::string("Requested element was not found in the current sequence.");
                    return result;
                }
                while (static_cast<int>(element->GetEffectLayerCount()) <= item.layerNumber) {
                    if (element->AddEffectLayer() == nullptr) {
                        result.failedItemIndex = static_cast<int>(i);
                        result.errorCode = std::string("INTERNAL_ERROR");
                        result.errorMessage = std::string("Unable to allocate the requested effect layer.");
                        return result;
                    }
                }
                elements.push_back(element);
            }

            bool modified = false;
            result.items.reserve(request.effects.size());
            for (std::size_t i = 0; i < request.effects.size(); ++i) {
                const auto& item = request.effects[i];
                Element* element = elements[i];
                EffectLayer* layer = element->GetEffectLayer(item.layerNumber);
                if (layer == nullptr) {
                    result.failedItemIndex = static_cast<int>(i);
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Requested layer was not available after allocation.");
                    return result;
                }

                api::models::EffectBatchItemResult itemResult;
                itemResult.elementName = item.elementName;
                itemResult.layerNumber = item.layerNumber;
                itemResult.effectName = item.effectName;
                itemResult.startMs = item.startMs;
                itemResult.endMs = item.endMs;
                itemResult.clearExisting = item.clearExisting;

                if (item.clearExisting) {
                    std::vector<int> indexesToDelete;
                    for (int effectIndex = 0; effectIndex < layer->GetEffectCount(); ++effectIndex) {
                        Effect* effect = layer->GetEffect(effectIndex);
                        if (effect == nullptr) continue;
                        if (effect->GetEndTimeMS() < item.startMs || effect->GetStartTimeMS() > item.endMs) continue;
                        indexesToDelete.push_back(effectIndex);
                    }
                    for (auto it = indexesToDelete.rbegin(); it != indexesToDelete.rend(); ++it) {
                        layer->DeleteEffectByIndex(*it);
                        itemResult.clearedEffectCount++;
                        result.clearedEffectCount++;
                        modified = true;
                    }
                }

                Effect* effect = layer->AddEffect(0, item.effectName, item.settings, item.palette, item.startMs, item.endMs, EFFECT_NOT_SELECTED, false);
                if (effect == nullptr) {
                    result.failedItemIndex = static_cast<int>(i);
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Effect could not be created.");
                    return result;
                }

                itemResult.created = true;
                result.createdCount++;
                modified = true;
                result.items.push_back(std::move(itemResult));
            }

            if (modified) {
                _frame->MarkEffectsFileDirty();
                wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
                wxPostEvent(_frame, eventRowHeaderChanged);
            }

            result.ok = true;
            return result;
        });
    }

    [[nodiscard]] api::models::EffectWindowSummary readEffectsWindow(const api::models::EffectWindowRequest& request) const {
        api::models::EffectWindowSummary summary;
        summary.elementName = request.elementName;
        summary.startMs = request.startMs;
        summary.endMs = request.endMs;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
        if (element == nullptr) {
            return summary;
        }

        summary.elementFound = true;
        for (size_t layerIndex = 0; layerIndex < element->GetEffectLayerCount(); ++layerIndex) {
            EffectLayer* layer = element->GetEffectLayer(static_cast<int>(layerIndex));
            if (layer == nullptr) {
                continue;
            }
            for (auto* effect : layer->GetAllEffectsByTime(request.startMs, request.endMs)) {
                if (effect == nullptr) {
                    continue;
                }
                summary.effects.push_back({
                    static_cast<int>(layerIndex),
                    effect->GetEffectName(),
                    effect->GetStartTimeMS(),
                    effect->GetEndTimeMS()
                });
            }
        }
        return summary;
    }

    [[nodiscard]] api::models::TimingMarksSummary readTimingMarks(const api::models::TimingMarksRequest& request) const {
        api::models::TimingMarksSummary summary;
        summary.trackName = request.trackName;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        TimingElement* track = _frame->GetSequenceElements().GetTimingElement(request.trackName);
        if (track == nullptr) {
            return summary;
        }

        summary.trackFound = true;
        for (size_t layerIndex = 0; layerIndex < track->GetEffectLayerCount(); ++layerIndex) {
            EffectLayer* layer = track->GetEffectLayer(static_cast<int>(layerIndex));
            if (layer == nullptr) {
                continue;
            }
            const auto effects = (request.startMs.has_value() && request.endMs.has_value())
                ? layer->GetAllEffectsByTime(*request.startMs, *request.endMs)
                : std::vector<Effect*>(layer->GetEffects().begin(), layer->GetEffects().end());
            for (auto* effect : effects) {
                if (effect == nullptr) {
                    continue;
                }
                if (request.startMs.has_value() && effect->GetEndTimeMS() < *request.startMs) {
                    continue;
                }
                if (request.endMs.has_value() && effect->GetStartTimeMS() > *request.endMs) {
                    continue;
                }
                summary.marks.push_back({
                    effect->GetStartTimeMS(),
                    effect->GetEndTimeMS(),
                    effect->GetEffectName(),
                    static_cast<int>(layerIndex)
                });
            }
        }
        return summary;
    }

    [[nodiscard]] api::models::EnsureTimingTrackResult ensureTimingTrack(const api::models::EnsureTimingTrackRequest& request) const {
        return RunOnMainThread<api::models::EnsureTimingTrackResult>([this, request]() {
            api::models::EnsureTimingTrackResult result;
            result.requestedTrackName = request.trackName;
            result.subType = request.subType;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            if (TimingElement* existing = _frame->GetSequenceElements().GetTimingElement(request.trackName); existing != nullptr) {
                result.actualTrackName = existing->GetName();
                if (result.subType.empty()) result.subType = existing->GetSubType();
                return result;
            }
            TimingElement* created = _frame->AddTimingElement(request.trackName, request.subType);
            if (created != nullptr) {
                result.created = true;
                result.actualTrackName = created->GetName();
                result.subType = created->GetSubType();
            }
            return result;
        });
    }

    [[nodiscard]] api::models::AddTimingMarksResult addTimingMarks(const api::models::AddTimingMarksRequest& request) const {
        return RunOnMainThread<api::models::AddTimingMarksResult>([this, request]() {
            api::models::AddTimingMarksResult result;
            result.requestedTrackName = request.trackName;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            TimingElement* track = _frame->GetSequenceElements().GetTimingElement(request.trackName);
            if (track == nullptr) {
                track = _frame->AddTimingElement(request.trackName, request.subType);
                result.trackCreated = track != nullptr;
            }
            if (track == nullptr) return result;
            result.trackFound = true;
            result.actualTrackName = track->GetName();
            EffectLayer* layer = track->GetEffectLayer(0);
            if (layer == nullptr) layer = track->AddEffectLayer();
            if (layer == nullptr) return result;
            if (request.replaceExisting) layer->RemoveAllEffects(nullptr);
            for (const auto& mark : request.marks) {
                layer->AddEffect(0, mark.label, "", "", mark.startMs, mark.endMs, EFFECT_NOT_SELECTED, false);
                result.addedMarkCount++;
            }
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            return result;
        });
    }

    [[nodiscard]] api::models::TimingTracksSummary readTimingTracks() const {
        api::models::TimingTracksSummary summary;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        int trackCount = _frame->GetSequenceElements().GetNumberOfTimingElements();
        summary.tracks.reserve(trackCount);
        for (int i = 0; i < trackCount; ++i) {
            TimingElement* track = _frame->GetSequenceElements().GetTimingElement(i);
            if (track == nullptr) {
                continue;
            }
            int markCount = 0;
            if (auto* layer0 = track->GetEffectLayer(0); layer0 != nullptr) {
                markCount = layer0->GetEffectCount();
            }
            summary.tracks.push_back({
                track->GetName(),
                track->IsFixedTiming() ? "fixed" : "variable",
                markCount
            });
        }
        return summary;
    }

private:
    [[nodiscard]] static api::models::LayoutGroupMemberSummary makeLayoutGroupMemberSummary(Model* model) {
        api::models::LayoutGroupMemberSummary summary;
        if (model == nullptr) {
            return summary;
        }
        summary.id = model->GetFullName();
        summary.name = model->GetName();
        summary.type = DisplayAsTypeToString(model->GetDisplayAs());
        summary.isGroup = model->GetDisplayAs() == DisplayAsType::ModelGroup;
        summary.isSubmodel = model->GetDisplayAs() == DisplayAsType::SubModel;
        summary.active = model->IsActive();
        return summary;
    }

    template <typename Result, typename Fn>
    [[nodiscard]] Result RunOnMainThread(Fn&& fn) const {
        if (_frame == nullptr) {
            return Result{};
        }
        if (wxIsMainThread()) {
            return fn();
        }
        auto promise = std::make_shared<std::promise<Result>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([promise, fn = std::forward<Fn>(fn)]() mutable {
            promise->set_value(fn());
        });
        return future.get();
    }

    xLightsFrame* _frame;
};

} // namespace xLightsDesigner
