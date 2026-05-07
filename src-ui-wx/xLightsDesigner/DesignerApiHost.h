#pragma once

#include <future>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <wx/filename.h>
#include <wx/base64.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/toplevel.h>
#include <memory>
#include <cmath>
#include <algorithm>
#include <set>

#include "FSEQFile.h"
#include "AudioManager.h"
#include "SequenceFile.h"
#include "xLightsMain.h"
#include "models/CustomModel.h"
#include "models/DisplayAsType.h"
#include "models/ModelGroup.h"
#include "models/OutputModelManager.h"
#include "models/SubModel.h"
#include "ExternalHooks.h"
#include "DesignerDiagnostics.h"
#include "DesignerApiRuntime.h"
#include "DesignerLaunchPolicy.h"
#include "api/models/EffectModels.h"
#include "api/models/ElementModels.h"
#include "api/models/LayoutModels.h"
#include "api/models/MediaModels.h"
#include "api/models/SequenceModels.h"
#include "api/models/TimingModels.h"
#include "api/transport/ErrorCatalog.h"

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

inline int ReadDesignerApiEnvIntMs(const char* name, int fallbackMs, int minimumMs = 1000) {
    if (name == nullptr || *name == '\0') {
        return fallbackMs;
    }
    if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
        try {
            return std::max(minimumMs, std::stoi(value));
        } catch (...) {
            return fallbackMs;
        }
    }
    return fallbackMs;
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
        wxFileName adjacent(xsqPath);
        adjacent.SetExt("fseq");
        if (wxFileExists(adjacent.GetFullPath())) {
            return adjacent.GetFullPath().ToStdString();
        }
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
    return output.GetFullPath();
}

inline bool WaitDesignerRenderComplete(xLightsFrame* frame) {
    if (frame == nullptr) {
        return false;
    }

    // RenderAll schedules asynchronous render work. The owned API must not
    // write the FSEQ until that work has populated sequence data for all frames.
    const int maxWaitMs = ReadDesignerApiEnvIntMs("XLIGHTS_DESIGNER_RENDER_WAIT_MS", 600000);
    const int maxAttempts = std::max(1, maxWaitMs / 25);
    for (int attempt = 0; attempt < maxAttempts && frame->ProgressBar != nullptr && frame->ProgressBar->IsShown(); ++attempt) {
        wxMilliSleep(25);
        wxYieldIfNeeded();
    }
    return frame->ProgressBar == nullptr || !frame->ProgressBar->IsShown();
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

inline std::filesystem::path ResolveOwnedAccessTarget(const std::string& path) {
    return xLightsDesigner::ResolveLaunchAccessTarget(path);
}

inline bool IsPathWithinRoot(const std::filesystem::path& target, const std::filesystem::path& root) {
    return xLightsDesigner::IsLaunchPathWithinRoot(target, root);
}

inline bool IsOwnedTrustedRootPath(const std::string& path) {
    return xLightsDesigner::IsLaunchTrustedRootPath(path);
}

inline void CollectDesignerWindowButtonLabels(wxWindow* window, nlohmann::json& labels) {
    if (window == nullptr) {
        return;
    }
    if (auto* button = wxDynamicCast(window, wxButton)) {
        const auto label = button->GetLabelText().ToStdString();
        if (!label.empty()) {
            labels.push_back(label);
        }
    }
    for (wxWindowList::compatibility_iterator child = window->GetChildren().GetFirst(); child; child = child->GetNext()) {
        CollectDesignerWindowButtonLabels(child->GetData(), labels);
    }
}

inline nlohmann::json BuildDesignerModalStateJson(xLightsFrame* frame) {
    nlohmann::json windows = nlohmann::json::array();
    std::size_t modalCount = 0;
    std::size_t shownDialogCount = 0;

    for (wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext()) {
        wxWindow* window = node->GetData();
        if (window == nullptr || !window->IsShown()) {
            continue;
        }

        auto* topLevel = wxDynamicCast(window, wxTopLevelWindow);
        auto* dialog = wxDynamicCast(window, wxDialog);
        const bool isDialog = dialog != nullptr;
        const bool isModal = dialog != nullptr && dialog->IsModal();
        if (!isDialog) {
            continue;
        }

        if (isDialog) {
            shownDialogCount++;
        }
        if (isModal) {
            modalCount++;
        }

        nlohmann::json buttons = nlohmann::json::array();
        CollectDesignerWindowButtonLabels(window, buttons);
        const wxClassInfo* classInfo = window->GetClassInfo();
        windows.push_back(nlohmann::json{
            {"title", topLevel != nullptr ? topLevel->GetTitle().ToStdString() : std::string()},
            {"className", classInfo != nullptr ? wxString(classInfo->GetClassName()).ToStdString() : std::string()},
            {"isDialog", isDialog},
            {"isModal", isModal},
            {"buttons", buttons}
        });
    }

    return nlohmann::json{
        {"observed", true},
        {"blocked", modalCount > 0},
        {"modalCount", modalCount},
        {"shownDialogCount", shownDialogCount},
        {"windows", windows}
    };
}

inline bool HasOwnedTrustedRootAccess(const std::string& path, bool enforceWritable) {
    return xLightsDesigner::HasLaunchTrustedRootAccess(path, enforceWritable);
}

inline bool ObtainOwnedApiAccessToPath(const std::string& path, bool enforceWritable = false) {
    if (ObtainAccessToURL(path, enforceWritable)) {
        return true;
    }
    return HasOwnedTrustedRootAccess(path, enforceWritable);
}

inline bool HasOwnedShowFolderAccess(xLightsFrame* frame, const std::string& path, bool enforceWritable = false) {
    if (frame == nullptr || path.empty() || frame->CurrentDir.empty()) {
        return false;
    }

    const auto target = ResolveOwnedAccessTarget(path);
    const auto showRoot = ResolveOwnedAccessTarget(frame->CurrentDir.ToStdString());
    if (target.empty() || showRoot.empty() || !IsPathWithinRoot(target, showRoot)) {
        return false;
    }

    if (!enforceWritable) {
        return true;
    }

    std::error_code ec;
    const auto probeDir = std::filesystem::is_directory(target, ec) ? target : target.parent_path();
    if (ec || probeDir.empty() || !std::filesystem::exists(probeDir, ec)) {
        return false;
    }

    const auto probe = probeDir / ".xld-owned-show-write-test";
    std::ofstream out(probe.string(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.close();
    std::filesystem::remove(probe, ec);
    return true;
}

inline bool FilesHaveEqualContents(const std::filesystem::path& left, const std::filesystem::path& right) {
    std::error_code ec;
    if (!std::filesystem::exists(left, ec) || !std::filesystem::exists(right, ec)) {
        return false;
    }
    if (std::filesystem::file_size(left, ec) != std::filesystem::file_size(right, ec)) {
        return false;
    }

    std::ifstream lhs(left, std::ios::binary);
    std::ifstream rhs(right, std::ios::binary);
    if (!lhs.is_open() || !rhs.is_open()) {
        return false;
    }

    constexpr std::size_t kBufferSize = 8192;
    char leftBuffer[kBufferSize];
    char rightBuffer[kBufferSize];
    while (lhs && rhs) {
        lhs.read(leftBuffer, static_cast<std::streamsize>(kBufferSize));
        rhs.read(rightBuffer, static_cast<std::streamsize>(kBufferSize));
        if (lhs.gcount() != rhs.gcount()) {
            return false;
        }
        if (std::memcmp(leftBuffer, rightBuffer, static_cast<std::size_t>(lhs.gcount())) != 0) {
            return false;
        }
    }
    return lhs.eof() && rhs.eof();
}

inline bool CanReuseCurrentShowForTrustedWorkspace(xLightsFrame* frame, const wxString& targetShowDir) {
    if (frame == nullptr || targetShowDir.empty() || frame->CurrentDir.empty()) {
        return false;
    }

    const auto currentDir = ResolveOwnedAccessTarget(frame->CurrentDir.ToStdString());
    const auto targetDir = ResolveOwnedAccessTarget(targetShowDir.ToStdString());
    if (currentDir.empty() || targetDir.empty()) {
        return false;
    }
    if (currentDir == targetDir) {
        return true;
    }

    const auto currentRgb = currentDir / XLIGHTS_RGBEFFECTS_FILE;
    const auto targetRgb = targetDir / XLIGHTS_RGBEFFECTS_FILE;
    const auto currentNet = currentDir / XLIGHTS_NETWORK_FILE;
    const auto targetNet = targetDir / XLIGHTS_NETWORK_FILE;

    return FilesHaveEqualContents(currentRgb, targetRgb) &&
           FilesHaveEqualContents(currentNet, targetNet);
}

inline bool PrepareDesignerShowDirectoryForSequence(xLightsFrame* frame, const std::string& sequenceFile) {
    if (frame == nullptr || sequenceFile.empty()) {
        AppendDesignerDiagnostic("PrepareDesignerShowDirectoryForSequence invalid frameOrSequence");
        return false;
    }
    const wxString targetShowDir = FindDesignerShowDirectoryForSequence(sequenceFile);
    if (targetShowDir.empty()) {
        AppendDesignerDiagnostic(std::string("PrepareDesignerShowDirectoryForSequence noTargetShowDir file=") + sequenceFile);
        return true;
    }
    if (frame->CurrentDir == targetShowDir) {
        AppendDesignerDiagnostic(std::string("PrepareDesignerShowDirectoryForSequence alreadyCurrent current=") +
                                 frame->CurrentDir.ToStdString() + " target=" + targetShowDir.ToStdString());
        return true;
    }
    const std::string targetShowDirUtf8 = targetShowDir.ToStdString();
    const bool accessOk = ObtainOwnedApiAccessToPath(targetShowDirUtf8, true);
    const bool trustedWorkspace = IsOwnedTrustedRootPath(targetShowDirUtf8);
    const bool reusable = trustedWorkspace && CanReuseCurrentShowForTrustedWorkspace(frame, targetShowDir);
    AppendDesignerDiagnostic(std::string("PrepareDesignerShowDirectoryForSequence current=") +
                             frame->CurrentDir.ToStdString() +
                             " target=" + targetShowDirUtf8 +
                             " accessOk=" + (accessOk ? "1" : "0") +
                             " trustedWorkspace=" + (trustedWorkspace ? "1" : "0") +
                             " reusable=" + (reusable ? "1" : "0"));
    if (!accessOk) {
        return false;
    }
    if (reusable) {
        return true;
    }
    const bool setDirOk = frame->SetDir(targetShowDir, !trustedWorkspace);
    AppendDesignerDiagnostic(std::string("PrepareDesignerShowDirectoryForSequence setDirOk=") + (setDirOk ? "1" : "0"));
    return setDirOk;
}

struct OwnedSequenceOpenGuardState {
    bool renderMode = false;
    bool promptBatchRenderIssues = true;
};

inline OwnedSequenceOpenGuardState EnterOwnedSequenceOpenState(xLightsFrame* frame,
                                                               const std::string& sequenceFile,
                                                               bool force) {
    OwnedSequenceOpenGuardState state;
    if (frame == nullptr) {
        return state;
    }

    state.renderMode = frame->_renderMode;
    state.promptBatchRenderIssues = frame->_promptBatchRenderIssues;

    // Owned API sequence automation should be prompt-free. Mirror the legacy
    // automation path by suppressing batch-render prompts while the open runs.
    frame->_renderMode = true;
    frame->_promptBatchRenderIssues = false;

    if (!force) {
        return state;
    }

    if (frame->CurrentSeqXmlFile != nullptr) {
        frame->mSavedChangeCount = frame->GetSequenceElements().GetChangeCount();
    }

    const wxString targetShowDir = FindDesignerShowDirectoryForSequence(sequenceFile);
    if (!targetShowDir.empty() && IsOwnedTrustedRootPath(targetShowDir.ToStdString())) {
        frame->UnsavedRgbEffectsChanges = false;
        frame->UnsavedNetworkChanges = false;
    }

    return state;
}

inline void ExitOwnedSequenceOpenState(xLightsFrame* frame, const OwnedSequenceOpenGuardState& state) {
    if (frame == nullptr) {
        return;
    }
    frame->_renderMode = state.renderMode;
    frame->_promptBatchRenderIssues = state.promptBatchRenderIssues;
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
        settings.supportsModelBlending = _frame->CurrentSeqXmlFile->supportsModelBlending();
        settings.hasUnsavedChanges = (_frame->mSavedChangeCount != _frame->GetSequenceElements().GetChangeCount());
        return settings;
    }

    [[nodiscard]] api::models::SequenceSettingsUpdateResult updateSequenceSettings(const api::models::SequenceSettingsUpdateRequest& request) const {
        return RunOnMainThread<api::models::SequenceSettingsUpdateResult>([this, request]() {
            api::models::SequenceSettingsUpdateResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }
            if (_frame->IsReadOnlyMode()) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "Sequence settings cannot be updated in read only mode.";
                return result;
            }

            bool changed = false;

            if (request.sequenceType.has_value() &&
                request.sequenceType.value() != _frame->CurrentSeqXmlFile->GetSequenceType()) {
                _frame->CurrentSeqXmlFile->SetSequenceType(request.sequenceType.value());
                changed = true;
            }

            if (request.durationMs.has_value() &&
                request.durationMs.value() != _frame->CurrentSeqXmlFile->GetSequenceDurationMS()) {
                _frame->CurrentSeqXmlFile->SetSequenceDuration(static_cast<double>(request.durationMs.value()) / 1000.0);
                _frame->UpdateSequenceLength();
                _frame->SetSequenceEnd(_frame->CurrentSeqXmlFile->GetSequenceDurationMS());
                changed = true;
            }

            if (request.frameMs.has_value() &&
                request.frameMs.value() != _frame->CurrentSeqXmlFile->GetFrameMS()) {
                const wxString timing = wxString::Format("%d ms", request.frameMs.value());
                _frame->CurrentSeqXmlFile->SetSequenceTiming(timing);
                _frame->SetSequenceTiming(request.frameMs.value());
                if (_frame->CurrentSeqXmlFile->GetMedia() != nullptr) {
                    _frame->CurrentSeqXmlFile->GetMedia()->SetFrameInterval(request.frameMs.value());
                }
                changed = true;
            }

            if (request.supportsModelBlending.has_value() &&
                request.supportsModelBlending.value() != _frame->CurrentSeqXmlFile->supportsModelBlending()) {
                _frame->CurrentSeqXmlFile->setSupportsModelBlending(request.supportsModelBlending.value());
                _frame->GetSequenceElements().SetSupportsModelBlending(request.supportsModelBlending.value());
                changed = true;
            }

            auto applyHeader = [this, &changed](HEADER_INFO_TYPES type, const std::optional<std::string>& value) {
                if (!value.has_value()) {
                    return;
                }
                const wxString next = wxString::FromUTF8(value.value());
                if (_frame->CurrentSeqXmlFile->GetHeaderInfo(type) == next) {
                    return;
                }
                _frame->CurrentSeqXmlFile->SetHeaderInfo(type, next);
                changed = true;
            };

            applyHeader(HEADER_INFO_TYPES::AUTHOR, request.metadataAuthor);
            applyHeader(HEADER_INFO_TYPES::AUTHOR_EMAIL, request.metadataAuthorEmail);
            applyHeader(HEADER_INFO_TYPES::WEBSITE, request.metadataWebsite);
            applyHeader(HEADER_INFO_TYPES::SONG, request.metadataSong);
            applyHeader(HEADER_INFO_TYPES::ARTIST, request.metadataArtist);
            applyHeader(HEADER_INFO_TYPES::ALBUM, request.metadataAlbum);
            applyHeader(HEADER_INFO_TYPES::URL, request.metadataMusicUrl);
            applyHeader(HEADER_INFO_TYPES::COMMENT, request.metadataComment);

            result.updated = true;
            result.settings = readSequenceSettings();
            if (changed) {
                _frame->GetSequenceElements().IncrementChangeCount(nullptr);
                result.settings = readSequenceSettings();
            }
            return result;
        });
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

    [[nodiscard]] nlohmann::json readModalState() const {
        if (_frame == nullptr) {
            return nlohmann::json{
                {"observed", false},
                {"blocked", false},
                {"modalCount", 0},
                {"shownDialogCount", 0},
                {"windows", nlohmann::json::array()},
                {"error", "xLights frame is unavailable."}
            };
        }
        if (wxIsMainThread()) {
            return detail::BuildDesignerModalStateJson(_frame);
        }

        auto promise = std::make_shared<std::promise<nlohmann::json>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([promise, frame]() mutable {
            promise->set_value(detail::BuildDesignerModalStateJson(frame));
        });
        if (future.wait_for(std::chrono::milliseconds(1500)) != std::future_status::ready) {
            return nlohmann::json{
                {"observed", false},
                {"blocked", false},
                {"uiThreadResponsive", false},
                {"modalCount", 0},
                {"shownDialogCount", 0},
                {"windows", nlohmann::json::array()},
                {"error", "Timed out waiting for the xLights UI thread to report modal state."}
            };
        }
        return future.get();
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
            const auto guard = detail::EnterOwnedSequenceOpenState(_frame, request.file, request.force);
            if (!detail::ObtainOwnedApiAccessToPath(request.file, false)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain access to the requested sequence file.";
                return result;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(_frame, request.file)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                result.errorCode = "SHOW_DIRECTORY_FAILED";
                result.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                return result;
            }
            _frame->OpenSequence(wxString::FromUTF8(request.file), nullptr);
            detail::ExitOwnedSequenceOpenState(_frame, guard);
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
        const bool force = request.force;
        frame->CallAfter([this, promise, requestedFile, force]() mutable {
            api::models::SequenceOpenResult callbackResult;
            callbackResult.requestedPath = requestedFile;
            const auto guard = detail::EnterOwnedSequenceOpenState(_frame, requestedFile, force);
            if (!detail::ObtainOwnedApiAccessToPath(requestedFile, false)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                callbackResult.errorCode = "SEQUENCE_ACCESS_DENIED";
                callbackResult.errorMessage = "Unable to obtain access to the requested sequence file.";
                promise->set_value(callbackResult);
                return;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(_frame, requestedFile)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                callbackResult.errorCode = "SHOW_DIRECTORY_FAILED";
                callbackResult.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                promise->set_value(callbackResult);
                return;
            }
            _frame->OpenSequence(wxString::FromUTF8(requestedFile), nullptr);
            detail::ExitOwnedSequenceOpenState(_frame, guard);
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
        const int openWaitMs = detail::ReadDesignerApiEnvIntMs("XLIGHTS_DESIGNER_SEQUENCE_OPEN_WAIT_MS", 600000);
        if (future.wait_for(std::chrono::milliseconds(openWaitMs)) != std::future_status::ready) {
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
            if (!detail::ObtainOwnedApiAccessToPath(targetAccessPath, true)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain write access to the requested sequence path.";
                return result;
            }
            if (!request.mediaFile.empty() && !detail::ObtainOwnedApiAccessToPath(request.mediaFile, false)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain access to the requested media file.";
                return result;
            }

            const std::string mediaFile = request.mediaFile == "null" ? std::string() : request.mediaFile;
            const std::string view = request.view == "null" ? std::string() : request.view;
            const auto guard = detail::EnterOwnedSequenceOpenState(_frame, request.file, true);
            _frame->NewSequence(mediaFile, static_cast<uint32_t>(request.durationMs > 0 ? request.durationMs : 0), static_cast<uint32_t>(request.frameMs > 0 ? request.frameMs : 25), view);
            detail::ExitOwnedSequenceOpenState(_frame, guard);
            _frame->EnableSequenceControls(true);
            if (_frame->CurrentSeqXmlFile == nullptr) {
                result.errorCode = "CREATE_FAILED";
                result.errorMessage = "Failed to create the requested sequence.";
                return result;
            }

            wxFileName fseqFile(targetFile);
            fseqFile.SetExt("fseq");
            xLightsFrame::xlightsFilename = fseqFile.GetFullPath();
            _frame->CurrentSeqXmlFile->SetFullPath(targetFile.GetFullPath().ToStdString());
            _frame->_renderCache.SetSequence(_frame->renderCacheDirectory, targetFile.GetName());

            {
                std::unique_lock<std::mutex> lock(_frame->saveLock);
                if (!_frame->CurrentSeqXmlFile->Save(_frame->GetSequenceElements())) {
                    result.errorCode = "CREATE_FAILED";
                    result.errorMessage = "Failed to save the requested sequence file.";
                    return result;
                }
                _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
                _frame->mLastAutosaveCount = _frame->mSavedChangeCount;
            }
            _frame->AddToMRU(targetFile.GetFullPath().ToStdString());
            _frame->UpdateRecentFilesList(false);
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
            result.errorCode = "SEQUENCE_NOT_OPEN";
            result.errorMessage = "No sequence is open.";
            return result;
        }

        auto finalizeResult = [this]() {
            api::models::SequenceSaveResult saveResult;
            saveResult.sequence = readOpenSequence();
            saveResult.saved = saveResult.sequence.isOpen;
            return saveResult;
        };

        if (_frame->CurrentSeqXmlFile->GetFullPath().empty() || _frame->IsReadOnlyMode()) {
            result.errorCode = "SEQUENCE_SAVE_UNAVAILABLE";
            result.errorMessage = "The current sequence has no writable path or is in read-only mode.";
            return result;
        }

        if (_frame->mSavedChangeCount == _frame->GetSequenceElements().GetChangeCount()) {
            return finalizeResult();
        }

        const std::string currentSequencePath = _frame->CurrentSeqXmlFile->GetFullPath();
        if (!detail::HasOwnedShowFolderAccess(_frame, currentSequencePath, true) &&
            !detail::ObtainOwnedApiAccessToPath(currentSequencePath, true)) {
            result.errorCode = "SEQUENCE_ACCESS_DENIED";
            result.errorMessage = "Unable to obtain write access to the current sequence path.";
            return result;
        }

        auto performSave = [this, &result]() {
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
            if (!ok) {
                result.errorCode = "SEQUENCE_SAVE_FAILED";
                result.errorMessage = "Unable to save the current sequence file.";
                return false;
            }

            _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
            _frame->mLastAutosaveCount = _frame->mSavedChangeCount;
            return true;
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
            const auto fseqPath = detail::ResolveDesignerRenderedFseqPath(_frame);
            if (!fseqPath.empty()) {
                renderResult.fseqPath = fseqPath;
            }
            return renderResult;
        };

        auto performRender = [this]() {
            _frame->RenderAll();
            if (!detail::WaitDesignerRenderComplete(_frame)) {
                return false;
            }
            if (_frame->CurrentSeqXmlFile == nullptr) {
                return false;
            }

            const wxString renderedFseqPath = detail::BuildDesignerRenderedFseqPath(_frame);
            if (renderedFseqPath.empty()) {
                return false;
            }
            detail::ObtainOwnedApiAccessToPath(renderedFseqPath.ToStdString());
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

    [[nodiscard]] api::models::SequencePreviewVideoExportResult exportPreviewVideo(const api::models::SequencePreviewVideoExportRequest& request) const {
        return RunOnMainThread<api::models::SequencePreviewVideoExportResult>([this, request]() {
            api::models::SequencePreviewVideoExportResult result;
            result.file = request.file;
            result.sequence = readOpenSequence();
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr || !result.sequence.isOpen) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }
            if (request.file.empty()) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "sequence.exportPreviewVideo requires a target file path.";
                return result;
            }

            wxFileName outputFile(wxString::FromUTF8(request.file));
            if (outputFile.GetExt().empty()) {
                outputFile.SetExt("mp4");
            }
            const std::string outputPath = outputFile.GetFullPath().ToStdString();
            result.file = outputPath;
            const wxString outputDir = outputFile.GetPath();
            if (outputDir.empty() || !wxDirExists(outputDir)) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "Preview video output directory does not exist.";
                return result;
            }
            const std::string accessPath = outputDir.ToStdString();
            if (!detail::ObtainOwnedApiAccessToPath(accessPath, true)) {
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain write access to the requested preview video path.";
                return result;
            }

            if (request.renderFirst) {
                _frame->RenderAll();
                if (!detail::WaitDesignerRenderComplete(_frame)) {
                    result.errorCode = "SEQUENCE_RENDER_TIMEOUT";
                    result.errorMessage = "Timed out waiting for xLights to render before preview video export.";
                    return result;
                }
            }

            const bool exported = _frame->ExportVideoPreview(outputFile.GetFullPath(), false);
            result.sequence = readOpenSequence();
            if (!exported || !wxFileExists(outputFile.GetFullPath())) {
                result.errorCode = "PREVIEW_VIDEO_EXPORT_FAILED";
                result.errorMessage = "xLights failed to export the requested preview video.";
                return result;
            }

            result.exported = true;
            return result;
        });
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

    [[nodiscard]] api::models::MediaShowDirectoryResult setShowDirectory(const api::models::MediaShowDirectoryRequest& request) const {
        api::models::MediaShowDirectoryResult result;
        result.showDirectory = request.showDirectory;
        if (_frame == nullptr || request.showDirectory.empty()) {
            result.errorCode = "VALIDATION_ERROR";
            result.errorMessage = "showDirectory is required.";
            return result;
        }

        result.previousShowDirectory = _frame->CurrentDir.ToStdString();
        const wxString targetShowDir = wxString::FromUTF8(request.showDirectory);
        if (!wxFileName::DirExists(targetShowDir)) {
            result.errorCode = "SHOW_DIRECTORY_NOT_FOUND";
            result.errorMessage = "Show directory does not exist.";
            return result;
        }

        if (_frame->CurrentDir == targetShowDir) {
            result.changed = false;
            result.showDirectory = _frame->CurrentDir.ToStdString();
            return result;
        }

        if (_frame->CurrentSeqXmlFile != nullptr && !request.force) {
            result.errorCode = "SEQUENCE_OPEN";
            result.errorMessage = "A sequence is open. Pass force=true to close it and switch show directories.";
            return result;
        }

        if ((_frame->UnsavedRgbEffectsChanges || _frame->UnsavedNetworkChanges) && !request.force) {
            result.errorCode = "UNSAVED_CHANGES";
            result.errorMessage = "The current show directory has unsaved layout or network changes. Pass force=true to discard them and switch show directories.";
            return result;
        }

        if (!detail::ObtainOwnedApiAccessToPath(request.showDirectory, true)) {
            result.errorCode = "SHOW_DIRECTORY_ACCESS_DENIED";
            result.errorMessage = "Unable to obtain write access to the requested show directory.";
            return result;
        }

        auto switchOnMainThread = [this, request, targetShowDir]() {
            api::models::MediaShowDirectoryResult callbackResult;
            callbackResult.previousShowDirectory = _frame != nullptr ? _frame->CurrentDir.ToStdString() : std::string();
            callbackResult.showDirectory = request.showDirectory;
            if (_frame == nullptr) {
                callbackResult.errorCode = "VALIDATION_ERROR";
                callbackResult.errorMessage = "xLights frame is not available.";
                return callbackResult;
            }

            const bool hadSequence = _frame->CurrentSeqXmlFile != nullptr;
            const bool previousRenderMode = _frame->_renderMode;
            const bool previousPromptBatchRenderIssues = _frame->_promptBatchRenderIssues;
            if (request.force) {
                _frame->_renderMode = true;
                _frame->_promptBatchRenderIssues = false;
                if (_frame->CurrentSeqXmlFile != nullptr) {
                    _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
                }
                _frame->UnsavedRgbEffectsChanges = false;
                _frame->UnsavedNetworkChanges = false;
            }

            const bool switched = _frame->SetDir(targetShowDir, request.permanent);

            if (request.force) {
                _frame->_renderMode = previousRenderMode;
                _frame->_promptBatchRenderIssues = previousPromptBatchRenderIssues;
            }

            callbackResult.sequenceClosed = hadSequence && _frame->CurrentSeqXmlFile == nullptr;
            callbackResult.showDirectory = _frame->CurrentDir.ToStdString();
            callbackResult.changed = switched && _frame->CurrentDir == targetShowDir;
            if (!callbackResult.changed) {
                callbackResult.errorCode = "SHOW_DIRECTORY_SWITCH_FAILED";
                callbackResult.errorMessage = "xLights did not switch to the requested show directory.";
            }
            return callbackResult;
        };

        if (wxIsMainThread()) {
            return switchOnMainThread();
        }

        auto promise = std::make_shared<std::promise<api::models::MediaShowDirectoryResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([promise, switchOnMainThread]() mutable {
            promise->set_value(switchOnMainThread());
        });
        if (future.wait_for(std::chrono::seconds(90)) != std::future_status::ready) {
            result.errorCode = "SHOW_DIRECTORY_SWITCH_TIMEOUT";
            result.errorMessage = "Timed out waiting for xLights to switch show directories.";
            return result;
        }
        return future.get();
    }

    [[nodiscard]] api::models::MediaShowDirectoryResult requestShowDirectoryAccess(const api::models::MediaShowDirectoryRequest& request) const {
        api::models::MediaShowDirectoryResult result;
        result.showDirectory = request.showDirectory;
        if (_frame == nullptr || request.showDirectory.empty()) {
            result.errorCode = "VALIDATION_ERROR";
            result.errorMessage = "showDirectory is required.";
            return result;
        }

        result.previousShowDirectory = _frame->CurrentDir.ToStdString();
        const wxString targetShowDir = wxString::FromUTF8(request.showDirectory);
        if (!wxFileName::DirExists(targetShowDir)) {
            result.errorCode = "SHOW_DIRECTORY_NOT_FOUND";
            result.errorMessage = "Show directory does not exist.";
            return result;
        }

        auto promptOnMainThread = [this, request, targetShowDir]() {
            api::models::MediaShowDirectoryResult callbackResult;
            callbackResult.previousShowDirectory = _frame != nullptr ? _frame->CurrentDir.ToStdString() : std::string();
            callbackResult.showDirectory = request.showDirectory;
            if (_frame == nullptr) {
                callbackResult.errorCode = "VALIDATION_ERROR";
                callbackResult.errorMessage = "xLights frame is not available.";
                return callbackResult;
            }

            wxDirDialog dialog(_frame,
                               _("Select the project show folder for xLightsDesigner"),
                               targetShowDir,
                               wxDD_DEFAULT_STYLE,
                               wxDefaultPosition,
                               wxDefaultSize,
                               _T("wxDirDialog"));
            if (dialog.ShowModal() != wxID_OK) {
                callbackResult.errorCode = "SHOW_DIRECTORY_ACCESS_CANCELLED";
                callbackResult.errorMessage = "Show directory access request was cancelled.";
                return callbackResult;
            }

            const wxString selectedDir = dialog.GetPath();
            std::error_code normalizeError;
            const auto selectedNormalized = std::filesystem::weakly_canonical(
                std::filesystem::path(selectedDir.ToStdString()),
                normalizeError);
            std::error_code targetNormalizeError;
            const auto targetNormalized = std::filesystem::weakly_canonical(
                std::filesystem::path(targetShowDir.ToStdString()),
                targetNormalizeError);
            if (normalizeError || targetNormalizeError || selectedNormalized.empty() || targetNormalized.empty()) {
                callbackResult.errorCode = "SHOW_DIRECTORY_ACCESS_MISMATCH";
                callbackResult.errorMessage = "The selected folder could not be matched to the project show folder.";
                callbackResult.showDirectory = selectedDir.ToStdString();
                return callbackResult;
            }
            if (selectedNormalized != targetNormalized) {
                callbackResult.errorCode = "SHOW_DIRECTORY_ACCESS_MISMATCH";
                callbackResult.errorMessage = "The selected folder did not match the project show folder.";
                callbackResult.showDirectory = selectedDir.ToStdString();
                return callbackResult;
            }

            ObtainAccessToURL(selectedDir, true);
            const bool hadSequence = _frame->CurrentSeqXmlFile != nullptr;
            const bool previousRenderMode = _frame->_renderMode;
            const bool previousPromptBatchRenderIssues = _frame->_promptBatchRenderIssues;
            if (request.force) {
                _frame->_renderMode = true;
                _frame->_promptBatchRenderIssues = false;
                if (_frame->CurrentSeqXmlFile != nullptr) {
                    _frame->mSavedChangeCount = _frame->GetSequenceElements().GetChangeCount();
                }
                _frame->UnsavedRgbEffectsChanges = false;
                _frame->UnsavedNetworkChanges = false;
            }

            const bool switched = _frame->SetDir(targetShowDir, request.permanent);

            if (request.force) {
                _frame->_renderMode = previousRenderMode;
                _frame->_promptBatchRenderIssues = previousPromptBatchRenderIssues;
            }

            callbackResult.sequenceClosed = hadSequence && _frame->CurrentSeqXmlFile == nullptr;
            callbackResult.showDirectory = _frame->CurrentDir.ToStdString();
            callbackResult.changed = switched && _frame->CurrentDir == targetShowDir;
            if (!callbackResult.changed) {
                callbackResult.errorCode = "SHOW_DIRECTORY_SWITCH_FAILED";
                callbackResult.errorMessage = "xLights did not switch to the requested show directory.";
            }
            return callbackResult;
        };

        if (wxIsMainThread()) {
            return promptOnMainThread();
        }

        auto promise = std::make_shared<std::promise<api::models::MediaShowDirectoryResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        frame->CallAfter([promise, promptOnMainThread]() mutable {
            promise->set_value(promptOnMainThread());
        });
        if (future.wait_for(std::chrono::minutes(5)) != std::future_status::ready) {
            result.errorCode = "SHOW_DIRECTORY_ACCESS_TIMEOUT";
            result.errorMessage = "Timed out waiting for show directory access.";
            return result;
        }
        return future.get();
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

    [[nodiscard]] api::models::LayoutSubmodelsSummary readLayoutSubmodels() const {
        api::models::LayoutSubmodelsSummary summary;
        if (_frame == nullptr) {
            return summary;
        }

        for (auto it = _frame->AllModels.begin(); it != _frame->AllModels.end(); ++it) {
            const auto* model = it->second;
            if (model == nullptr || model->GetDisplayAs() == DisplayAsType::SubModel) {
                continue;
            }
            for (const auto* child : model->GetSubModels()) {
                const auto* submodel = dynamic_cast<const SubModel*>(child);
                if (submodel == nullptr) {
                    continue;
                }
                api::models::LayoutSubmodelSummary row;
                row.name = submodel->GetName();
                row.fullName = submodel->GetFullName();
                row.parentName = model->GetName();
                row.layoutGroup = submodel->GetLayoutGroup();
                row.layout = submodel->GetSubModelLayout();
                row.type = submodel->GetSubModelType();
                row.bufferStyle = submodel->GetSubModelBufferStyle();
                row.lines = submodel->GetSubModelLines();
                row.startChannel = static_cast<int>(submodel->GetFirstChannel()) + 1;
                row.endChannel = static_cast<int>(submodel->GetLastChannel()) + 1;
                row.nodeCount = static_cast<int>(submodel->GetNodeCount());
                row.vertical = submodel->IsVertical();
                row.ranges = submodel->IsRanges();
                summary.submodels.push_back(std::move(row));
            }
        }

        return summary;
    }

    [[nodiscard]] api::models::LayoutModelNodesSummary readLayoutModelNodes(const api::models::LayoutModelNodesRequest& request) const {
        api::models::LayoutModelNodesSummary summary;
        summary.includeBufferCoords = request.includeBufferCoords;
        summary.includeWorldCoords = request.includeWorldCoords;
        summary.includeScreenCoords = request.includeScreenCoords;
        if (_frame == nullptr || request.name.empty()) {
            return summary;
        }

        auto* model = _frame->AllModels.GetModel(request.name);
        if (model == nullptr) {
            return summary;
        }

        summary.found = true;
        summary.modelName = model->GetName();
        summary.isCustomModel = model->IsCustom();
        const auto& location = model->GetModelScreenLocation();
        const uint32_t nodeCount = model->GetNodeCount();
        summary.nodes.reserve(nodeCount);
        for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
            auto* node = model->GetNode(nodeIndex);
            if (node == nullptr) {
                continue;
            }

            api::models::LayoutModelNodeSummary row;
            row.nodeId = static_cast<int>(nodeIndex + 1);
            row.stringIndex = static_cast<int>(node->StringNum);
            row.name = model->GetNodeName(nodeIndex, true);
            for (const auto& coord : node->Coords) {
                api::models::LayoutNodeCoordSummary coordRow;
                if (request.includeBufferCoords) {
                    coordRow.bufferX = coord.bufX;
                    coordRow.bufferY = coord.bufY;
                }
                if (request.includeWorldCoords) {
                    float worldX = coord.screenX;
                    float worldY = coord.screenY;
                    float worldZ = coord.screenZ;
                    location.TranslatePoint(worldX, worldY, worldZ);
                    coordRow.worldX = static_cast<double>(worldX);
                    coordRow.worldY = static_cast<double>(worldY);
                    coordRow.worldZ = static_cast<double>(worldZ);
                }
                if (request.includeScreenCoords) {
                    coordRow.screenX = static_cast<double>(coord.screenX);
                    coordRow.screenY = static_cast<double>(coord.screenY);
                    coordRow.screenZ = static_cast<double>(coord.screenZ);
                }
                row.coords.push_back(std::move(coordRow));
            }
            summary.nodes.push_back(std::move(row));
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

    [[nodiscard]] api::models::CreateCustomModelResult createCustomModel(const api::models::CreateCustomModelRequest& request) const {
        return RunOnMainThread<api::models::CreateCustomModelResult>([this, request]() {
            api::models::CreateCustomModelResult result;
            result.modelName = request.name;
            result.width = request.width;
            result.height = request.height;
            result.depth = request.depth;
            result.nodeCount = static_cast<int>(request.nodes.size());

            if (_frame == nullptr) {
                result.errorMessage = "xLights frame is unavailable.";
                return result;
            }
            if (request.name.empty()) {
                result.errorMessage = "layout.createCustomModel requires a model name.";
                return result;
            }
            if (request.width <= 0 || request.height <= 0 || request.depth <= 0) {
                result.errorMessage = "layout.createCustomModel requires positive width, height, and depth.";
                return result;
            }
            if (request.stringCount <= 0) {
                result.errorMessage = "layout.createCustomModel requires a positive stringCount.";
                return result;
            }
            if (request.nodes.empty()) {
                result.errorMessage = "layout.createCustomModel requires at least one node.";
                return result;
            }
            if (_frame->AllModels.GetModel(request.name) != nullptr && !request.overwrite) {
                result.errorMessage = "A model with that name already exists. Set overwrite=true to replace it.";
                return result;
            }

            std::vector<std::vector<std::vector<int>>> modelData(
                static_cast<size_t>(request.depth),
                std::vector<std::vector<int>>(
                    static_cast<size_t>(request.height),
                    std::vector<int>(static_cast<size_t>(request.width), 0)));
            std::set<int> nodeNumbers;

            for (const auto& node : request.nodes) {
                if (node.x < 0 || node.x >= request.width ||
                    node.y < 0 || node.y >= request.height ||
                    node.z < 0 || node.z >= request.depth) {
                    result.errorMessage = "Custom model node coordinate is outside the declared dimensions.";
                    return result;
                }
                if (node.node <= 0) {
                    result.errorMessage = "Custom model node numbers must be positive.";
                    return result;
                }
                if (node.string <= 0 || node.string > request.stringCount) {
                    result.errorMessage = "Custom model node string values must be between 1 and stringCount.";
                    return result;
                }
                auto& cell = modelData[static_cast<size_t>(node.z)][static_cast<size_t>(node.y)][static_cast<size_t>(node.x)];
                if (cell != 0) {
                    result.errorMessage = "Custom model contains more than one node in the same cell.";
                    return result;
                }
                cell = node.node;
                nodeNumbers.insert(node.node);
            }

            if (nodeNumbers.size() != request.nodes.size()) {
                result.errorMessage = "Custom model node numbers must be unique.";
                return result;
            }

            const bool replacing = _frame->AllModels.GetModel(request.name) != nullptr;
            if (request.dryRun) {
                return result;
            }

            std::unique_ptr<Model> model(_frame->AllModels.CreateDefaultModel("Custom", request.startChannel.empty() ? "1" : request.startChannel));
            auto* customModel = dynamic_cast<CustomModel*>(model.get());
            if (customModel == nullptr) {
                result.errorMessage = "xLights did not create a CustomModel instance.";
                return result;
            }

            customModel->SetName(request.name);
            customModel->SetStartChannel(request.startChannel.empty() ? "1" : request.startChannel);
            customModel->SetNumStrings(std::max(1, request.stringCount));
            customModel->UpdateModel(request.width, request.height, request.depth, modelData);
            customModel->SetLayoutGroup(request.layoutGroup.empty() ? "Default" : request.layoutGroup);
            customModel->SetPosition(request.positionX, request.positionY);
            customModel->GetModelScreenLocation().SetMWidth(static_cast<float>(request.width));
            customModel->GetModelScreenLocation().SetMHeight(static_cast<float>(request.height));
            customModel->GetModelScreenLocation().SetMDepth(static_cast<float>(request.depth));

            if (replacing && !_frame->AllModels.Delete(request.name)) {
                result.errorMessage = "Existing model could not be replaced.";
                return result;
            }

            _frame->AllModels.AddModel(model.release());
            _frame->MarkModelsAsNeedingRender();
            if (_frame->GetOutputModelManager() != nullptr) {
                _frame->GetOutputModelManager()->AddASAPWork(
                    OutputModelManager::WORK_MODELS_REWORK_STARTCHANNELS |
                    OutputModelManager::WORK_CALCULATE_START_CHANNELS |
                    OutputModelManager::WORK_RGBEFFECTS_CHANGE |
                    OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER |
                    OutputModelManager::WORK_RELOAD_MODELLIST |
                    OutputModelManager::WORK_RELOAD_ALLMODELS |
                    OutputModelManager::WORK_REDRAW_LAYOUTPREVIEW,
                    "xLightsDesigner::createCustomModel",
                    nullptr,
                    nullptr,
                    request.name);
            }

            result.created = !replacing;
            result.updated = replacing;
            return result;
        });
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

    [[nodiscard]] api::models::DisplayElementOrderSummary readDisplayElementOrder() const {
        api::models::DisplayElementOrderSummary summary;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        auto& sequenceElements = _frame->GetSequenceElements();
        summary.elements.reserve(sequenceElements.GetElementCount(MASTER_VIEW));
        for (size_t i = 0; i < sequenceElements.GetElementCount(MASTER_VIEW); ++i) {
            Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
            if (element == nullptr) {
                continue;
            }
            summary.elements.push_back({
                element->GetFullName(),
                element->GetTypeDescription(),
                static_cast<int>(i)
            });
        }
        return summary;
    }

    [[nodiscard]] api::models::SetDisplayElementOrderResult setDisplayElementOrder(const api::models::SetDisplayElementOrderRequest& request) const {
        return RunOnMainThread<api::models::SetDisplayElementOrderResult>([this, request]() {
            api::models::SetDisplayElementOrderResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            auto& sequenceElements = _frame->GetSequenceElements();

            std::set<std::string> seen;
            for (const auto& id : request.orderedIds) {
                if (id.empty()) {
                    continue;
                }
                if (!seen.insert(id).second) {
                    result.duplicateIds.push_back(id);
                }
                if (sequenceElements.GetElement(id) == nullptr) {
                    result.missingIds.push_back(id);
                }
            }
            if (!result.duplicateIds.empty() || !result.missingIds.empty()) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Display order contains duplicate or unknown ids.");
                return result;
            }

            int destination = 0;
            for (const auto& id : request.orderedIds) {
                if (id.empty()) {
                    continue;
                }
                const int currentIndex = sequenceElements.GetElementIndex(id, MASTER_VIEW);
                if (currentIndex < 0) {
                    result.missingIds.push_back(id);
                    continue;
                }
                sequenceElements.MoveSequenceElement(currentIndex, destination, MASTER_VIEW);
                destination++;
            }
            sequenceElements.PopulateRowInformation();
            sequenceElements.PopulateVisibleRowInformation();
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.orderedCount = destination;
            return result;
        });
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

    [[nodiscard]] api::models::CloneEffectsResult cloneEffects(const api::models::CloneEffectsRequest& request) const {
        return RunOnMainThread<api::models::CloneEffectsResult>([this, request]() {
            api::models::CloneEffectsResult result;
            result.dryRun = request.dryRun;
            result.targetCount = static_cast<int>(request.targetElementNames.size());
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;

            Element* sourceElement = _frame->GetSequenceElements().GetElement(request.sourceElementName);
            if (sourceElement == nullptr) return result;
            result.sourceElementFound = true;

            std::vector<Element*> targetElements;
            targetElements.reserve(request.targetElementNames.size());
            for (const auto& targetName : request.targetElementNames) {
                Element* targetElement = _frame->GetSequenceElements().GetElement(targetName);
                if (targetElement == nullptr) {
                    result.missingTargetElements.push_back(targetName);
                    continue;
                }
                targetElements.push_back(targetElement);
            }
            if (!result.missingTargetElements.empty()) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("One or more target elements were not found in the current sequence.");
                return result;
            }

            struct SourceEffectSnapshot {
                int layerNumber = 0;
                int startMs = 0;
                int endMs = 0;
                std::string effectName;
                std::string settings;
                std::string palette;
            };
            std::vector<SourceEffectSnapshot> sourceEffects;
            for (size_t layerIndex = 0; layerIndex < sourceElement->GetEffectLayerCount(); ++layerIndex) {
                const int layerNumber = static_cast<int>(layerIndex);
                if (request.sourceLayerNumber >= 0 && layerNumber != request.sourceLayerNumber) continue;
                EffectLayer* layer = sourceElement->GetEffectLayer(layerNumber);
                if (layer == nullptr) continue;
                for (auto* effect : layer->GetAllEffectsByTime(request.sourceStartMs, request.sourceEndMs)) {
                    if (effect == nullptr) continue;
                    sourceEffects.push_back({
                        layerNumber,
                        effect->GetStartTimeMS(),
                        effect->GetEndTimeMS(),
                        effect->GetEffectName(),
                        effect->GetSettingsAsString(),
                        effect->GetPaletteAsString()
                    });
                }
            }
            result.matchedCount = static_cast<int>(sourceEffects.size());
            if (sourceEffects.empty()) {
                result.errorCode = std::string("NOT_FOUND");
                result.errorMessage = std::string("No source effects matched the requested selector.");
                return result;
            }

            const int targetStartMs = request.targetStartMs >= 0 ? request.targetStartMs : request.sourceStartMs;
            const int deltaMs = targetStartMs - request.sourceStartMs;

            auto targetLayerForSource = [&request](const SourceEffectSnapshot& source) {
                return request.targetLayerNumber >= 0
                    ? request.targetLayerNumber + (request.sourceLayerNumber >= 0 ? source.layerNumber - request.sourceLayerNumber : 0)
                    : source.layerNumber;
            };

            auto windowOverlaps = [](int aStartMs, int aEndMs, int bStartMs, int bEndMs) {
                return aStartMs < bEndMs && aEndMs > bStartMs;
            };

            for (std::size_t targetIndex = 0; targetIndex < targetElements.size(); ++targetIndex) {
                Element* targetElement = targetElements[targetIndex];
                const std::string& targetName = request.targetElementNames[targetIndex];
                for (const auto& source : sourceEffects) {
                    const int targetLayerNumber = targetLayerForSource(source);
                    const int nextStartMs = source.startMs + deltaMs;
                    const int nextEndMs = source.endMs + deltaMs;
                    if (targetLayerNumber < 0 || nextStartMs < 0 || nextEndMs < nextStartMs) {
                        result.errorCode = std::string("VALIDATION_ERROR");
                        result.errorMessage = std::string("Clone target layer and timing values must be valid.");
                        return result;
                    }
                    if (static_cast<int>(targetElement->GetEffectLayerCount()) <= targetLayerNumber) continue;

                    EffectLayer* targetLayer = targetElement->GetEffectLayer(targetLayerNumber);
                    if (targetLayer == nullptr) continue;
                    for (auto* existing : targetLayer->GetAllEffectsByTime(nextStartMs, nextEndMs)) {
                        if (existing == nullptr) continue;
                        if (!windowOverlaps(nextStartMs, nextEndMs, existing->GetStartTimeMS(), existing->GetEndTimeMS())) continue;
                        result.conflicts.push_back({
                            targetName,
                            targetLayerNumber,
                            nextStartMs,
                            nextEndMs,
                            existing->GetEffectName(),
                            existing->GetStartTimeMS(),
                            existing->GetEndTimeMS()
                        });
                    }
                }
            }
            result.conflictCount = static_cast<int>(result.conflicts.size());
            if (result.conflictCount > 0) {
                result.errorCode = std::string("TARGET_WINDOW_OCCUPIED");
                result.errorMessage = std::string("Clone target layer/time window overlaps existing effects. Choose an open layer or delete/update the existing effects explicitly.");
                return result;
            }

            for (std::size_t targetIndex = 0; targetIndex < targetElements.size(); ++targetIndex) {
                Element* targetElement = targetElements[targetIndex];
                const std::string& targetName = request.targetElementNames[targetIndex];
                for (const auto& source : sourceEffects) {
                    const int targetLayerNumber = targetLayerForSource(source);
                    const int nextStartMs = source.startMs + deltaMs;
                    const int nextEndMs = source.endMs + deltaMs;
                    if (targetLayerNumber < 0 || nextStartMs < 0 || nextEndMs < nextStartMs) {
                        result.errorCode = std::string("VALIDATION_ERROR");
                        result.errorMessage = std::string("Clone target layer and timing values must be valid.");
                        return result;
                    }
                    while (static_cast<int>(targetElement->GetEffectLayerCount()) <= targetLayerNumber) {
                        if (request.dryRun) break;
                        if (targetElement->AddEffectLayer() == nullptr) {
                            result.errorCode = std::string("INTERNAL_ERROR");
                            result.errorMessage = std::string("Unable to allocate the requested target effect layer.");
                            return result;
                        }
                    }
                    if (!request.dryRun && static_cast<int>(targetElement->GetEffectLayerCount()) <= targetLayerNumber) {
                        result.errorCode = std::string("INTERNAL_ERROR");
                        result.errorMessage = std::string("Requested target layer was not available after allocation.");
                        return result;
                    }
                    if (!request.dryRun) {
                        EffectLayer* targetLayer = targetElement->GetEffectLayer(targetLayerNumber);
                        if (targetLayer == nullptr) {
                            result.errorCode = std::string("INTERNAL_ERROR");
                            result.errorMessage = std::string("Requested target layer was not available.");
                            return result;
                        }
                        Effect* created = targetLayer->AddEffect(0, source.effectName, source.settings, source.palette, nextStartMs, nextEndMs, EFFECT_NOT_SELECTED, false);
                        if (created == nullptr) {
                            result.errorCode = std::string("INTERNAL_ERROR");
                            result.errorMessage = std::string("Cloned effect could not be created.");
                            return result;
                        }
                        result.createdCount++;
                    }
                    result.items.push_back({
                        request.sourceElementName,
                        source.layerNumber,
                        source.startMs,
                        source.endMs,
                        targetName,
                        targetLayerNumber,
                        nextStartMs,
                        nextEndMs,
                        source.effectName,
                        !request.dryRun
                    });
                }
            }

            if (request.mode == "move" && !request.dryRun) {
                for (int layerIndex = static_cast<int>(sourceElement->GetEffectLayerCount()) - 1; layerIndex >= 0; --layerIndex) {
                    if (request.sourceLayerNumber >= 0 && layerIndex != request.sourceLayerNumber) continue;
                    EffectLayer* layer = sourceElement->GetEffectLayer(layerIndex);
                    if (layer == nullptr) continue;
                    for (int effectIndex = layer->GetEffectCount() - 1; effectIndex >= 0; --effectIndex) {
                        Effect* effect = layer->GetEffect(effectIndex);
                        if (effect == nullptr) continue;
                        if (effect->GetEndTimeMS() < request.sourceStartMs || effect->GetStartTimeMS() > request.sourceEndMs) continue;
                        layer->DeleteEffectByIndex(effectIndex);
                        result.deletedSourceCount++;
                    }
                }
            }

            if (!request.dryRun && (result.createdCount > 0 || result.deletedSourceCount > 0)) {
                _frame->MarkEffectsFileDirty();
                wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
                wxPostEvent(_frame, eventRowHeaderChanged);
            }
            result.ok = true;
            return result;
        });
    }

    [[nodiscard]] api::models::UpdateEffectResult updateEffect(const api::models::UpdateEffectRequest& request) const {
        return RunOnMainThread<api::models::UpdateEffectResult>([this, request]() {
            api::models::UpdateEffectResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.selector.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;

            struct Match {
                int layerIndex;
                int effectIndex;
                Effect* effect;
            };
            std::vector<Match> matches;
            for (size_t layerIndex = 0; layerIndex < element->GetEffectLayerCount(); ++layerIndex) {
                if (request.selector.layerNumber.has_value() && static_cast<int>(layerIndex) != *request.selector.layerNumber) continue;
                EffectLayer* layer = element->GetEffectLayer(static_cast<int>(layerIndex));
                if (layer == nullptr) continue;
                for (int effectIndex = 0; effectIndex < layer->GetEffectCount(); ++effectIndex) {
                    Effect* effect = layer->GetEffect(effectIndex);
                    if (effect == nullptr) continue;
                    if (request.selector.effectId.has_value() && effect->GetID() != *request.selector.effectId) continue;
                    if (request.selector.startMs.has_value() && effect->GetStartTimeMS() != *request.selector.startMs) continue;
                    if (request.selector.endMs.has_value() && effect->GetEndTimeMS() != *request.selector.endMs) continue;
                    if (!request.selector.effectName.empty() && effect->GetEffectName() != request.selector.effectName) continue;
                    matches.push_back({static_cast<int>(layerIndex), effectIndex, effect});
                }
            }
            result.matchedCount = static_cast<int>(matches.size());
            if (matches.empty()) {
                result.errorCode = std::string("NOT_FOUND");
                result.errorMessage = std::string("No effects matched the requested selector.");
                return result;
            }
            if (matches.size() > 1 && !request.selector.effectId.has_value()) {
                result.errorCode = std::string("AMBIGUOUS_SELECTOR");
                result.errorMessage = std::string("Effect selector matched more than one effect. Use effectId or a narrower selector.");
                return result;
            }

            Match match = matches.front();
            EffectLayer* sourceLayer = element->GetEffectLayer(match.layerIndex);
            if (sourceLayer == nullptr) {
                result.errorCode = std::string("INTERNAL_ERROR");
                result.errorMessage = std::string("Matched effect layer is no longer available.");
                return result;
            }
            Effect* effect = match.effect;
            const int nextLayerIndex = request.layerNumber.value_or(match.layerIndex);
            const int nextStartMs = request.startMs.value_or(effect->GetStartTimeMS());
            const int nextEndMs = request.endMs.value_or(effect->GetEndTimeMS());
            const std::string nextEffectName = request.effectName.value_or(effect->GetEffectName());
            const std::string nextSettings = request.settings.value_or(effect->GetSettingsAsString());
            const std::string nextPalette = request.palette.value_or(effect->GetPaletteAsString());
            if (nextLayerIndex < 0 || nextStartMs < 0 || nextEndMs < nextStartMs) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Updated layer and timing values must be valid.");
                return result;
            }

            while (static_cast<int>(element->GetEffectLayerCount()) <= nextLayerIndex) {
                if (element->AddEffectLayer() == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Unable to allocate the requested target layer.");
                    return result;
                }
            }

            if (nextLayerIndex != match.layerIndex) {
                EffectLayer* targetLayer = element->GetEffectLayer(nextLayerIndex);
                if (targetLayer == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Requested target layer is not available.");
                    return result;
                }
                Effect* created = targetLayer->AddEffect(0, nextEffectName, nextSettings, nextPalette, nextStartMs, nextEndMs, EFFECT_NOT_SELECTED, false);
                if (created == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Updated effect could not be created on the target layer.");
                    return result;
                }
                sourceLayer->DeleteEffectByIndex(match.effectIndex);
            } else {
                effect->SetEffectName(nextEffectName);
                effect->SetStartTimeMS(nextStartMs);
                effect->SetEndTimeMS(nextEndMs);
                if (request.settings.has_value()) {
                    effect->SetSettings(nextSettings, true);
                }
                if (request.palette.has_value()) {
                    effect->SetPalette(nextPalette);
                }
            }

            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.updatedCount = 1;
            return result;
        });
    }

    [[nodiscard]] api::models::DeleteEffectsResult deleteEffects(const api::models::DeleteEffectsRequest& request) const {
        return RunOnMainThread<api::models::DeleteEffectsResult>([this, request]() {
            api::models::DeleteEffectsResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.selector.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;

            for (int layerIndex = static_cast<int>(element->GetEffectLayerCount()) - 1; layerIndex >= 0; --layerIndex) {
                if (request.selector.layerNumber.has_value() && layerIndex != *request.selector.layerNumber) continue;
                EffectLayer* layer = element->GetEffectLayer(layerIndex);
                if (layer == nullptr) continue;
                for (int effectIndex = layer->GetEffectCount() - 1; effectIndex >= 0; --effectIndex) {
                    Effect* effect = layer->GetEffect(effectIndex);
                    if (effect == nullptr) continue;
                    if (request.selector.effectId.has_value() && effect->GetID() != *request.selector.effectId) continue;
                    if (request.selector.startMs.has_value() && effect->GetStartTimeMS() != *request.selector.startMs) continue;
                    if (request.selector.endMs.has_value() && effect->GetEndTimeMS() != *request.selector.endMs) continue;
                    if (!request.selector.effectName.empty() && effect->GetEffectName() != request.selector.effectName) continue;
                    layer->DeleteEffectByIndex(effectIndex);
                    result.deletedCount++;
                }
            }
            if (result.deletedCount == 0) {
                result.errorCode = std::string("NOT_FOUND");
                result.errorMessage = std::string("No effects matched the requested selector.");
                return result;
            }
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            return result;
        });
    }

    [[nodiscard]] api::models::DeleteEffectLayerResult deleteEffectLayer(const api::models::DeleteEffectLayerRequest& request) const {
        return RunOnMainThread<api::models::DeleteEffectLayerResult>([this, request]() {
            api::models::DeleteEffectLayerResult result;
            result.layerNumber = request.layerNumber;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;
            result.layerCount = static_cast<int>(element->GetEffectLayerCount());
            if (request.layerNumber < 0 || request.layerNumber >= result.layerCount) return result;
            result.layerFound = true;
            if (result.layerCount <= 1) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Cannot delete the last effect layer.");
                return result;
            }
            EffectLayer* layer = element->GetEffectLayer(request.layerNumber);
            result.effectCount = layer == nullptr ? 0 : layer->GetEffectCount();
            if (result.effectCount > 0 && !request.force) {
                result.errorCode = std::string("LAYER_NOT_EMPTY");
                result.errorMessage = std::string("Layer contains effects. Pass force=true to delete it.");
                return result;
            }
            element->RemoveEffectLayer(request.layerNumber);
            _frame->GetSequenceElements().PopulateRowInformation();
            _frame->GetSequenceElements().PopulateVisibleRowInformation();
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.layerCount = static_cast<int>(element->GetEffectLayerCount());
            return result;
        });
    }

    [[nodiscard]] api::models::ReorderEffectLayerResult reorderEffectLayer(const api::models::ReorderEffectLayerRequest& request) const {
        return RunOnMainThread<api::models::ReorderEffectLayerResult>([this, request]() {
            api::models::ReorderEffectLayerResult result;
            result.fromLayer = request.fromLayer;
            result.toLayer = request.toLayer;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;
            result.layerCount = static_cast<int>(element->GetEffectLayerCount());
            if (request.fromLayer < 0 || request.toLayer < 0 || request.fromLayer >= result.layerCount || request.toLayer >= result.layerCount) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Layer indexes are outside the current layer range.");
                return result;
            }
            if (request.fromLayer == request.toLayer) {
                result.ok = true;
                return result;
            }

            struct EffectSnapshot {
                std::string effectName;
                std::string settings;
                std::string palette;
                int startMs = 0;
                int endMs = 0;
                int selected = EFFECT_NOT_SELECTED;
                bool isProtected = false;
            };
            struct LayerSnapshot {
                std::string layerName;
                std::vector<EffectSnapshot> effects;
            };

            std::vector<LayerSnapshot> layers;
            layers.reserve(static_cast<std::size_t>(result.layerCount));
            for (int layerIndex = 0; layerIndex < result.layerCount; ++layerIndex) {
                EffectLayer* layer = element->GetEffectLayer(layerIndex);
                if (layer == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Layer snapshot failed.");
                    return result;
                }
                LayerSnapshot layerSnapshot;
                layerSnapshot.layerName = layer->GetLayerName();
                layerSnapshot.effects.reserve(static_cast<std::size_t>(layer->GetEffectCount()));
                for (int effectIndex = 0; effectIndex < layer->GetEffectCount(); ++effectIndex) {
                    Effect* effect = layer->GetEffect(effectIndex);
                    if (effect == nullptr) continue;
                    layerSnapshot.effects.push_back({
                        effect->GetEffectName(),
                        effect->GetSettingsAsString(),
                        effect->GetPaletteAsString(),
                        effect->GetStartTimeMS(),
                        effect->GetEndTimeMS(),
                        effect->GetSelected(),
                        effect->GetProtected(),
                    });
                }
                layers.push_back(std::move(layerSnapshot));
            }

            LayerSnapshot movedLayer = std::move(layers[static_cast<std::size_t>(request.fromLayer)]);
            layers.erase(layers.begin() + request.fromLayer);
            layers.insert(layers.begin() + request.toLayer, std::move(movedLayer));

            for (int layerIndex = static_cast<int>(element->GetEffectLayerCount()) - 1; layerIndex >= 0; --layerIndex) {
                EffectLayer* layer = element->GetEffectLayer(layerIndex);
                if (layer == nullptr) continue;
                for (int effectIndex = layer->GetEffectCount() - 1; effectIndex >= 0; --effectIndex) {
                    Effect* effect = layer->GetEffect(effectIndex);
                    if (effect != nullptr) {
                        effect->SetLocked(false);
                    }
                    layer->DeleteEffectByIndex(effectIndex);
                }
                if (layerIndex > 0) {
                    element->RemoveEffectLayer(layerIndex);
                }
            }

            EffectLayer* firstLayer = element->GetEffectLayer(0);
            if (firstLayer == nullptr) {
                result.errorCode = std::string("INTERNAL_ERROR");
                result.errorMessage = std::string("Layer rebuild failed.");
                return result;
            }
            firstLayer->SetLayerName(layers.front().layerName);
            while (static_cast<int>(element->GetEffectLayerCount()) < static_cast<int>(layers.size())) {
                if (element->AddEffectLayer() == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Layer allocation failed.");
                    return result;
                }
            }
            for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
                EffectLayer* layer = element->GetEffectLayer(static_cast<int>(layerIndex));
                if (layer == nullptr) {
                    result.errorCode = std::string("INTERNAL_ERROR");
                    result.errorMessage = std::string("Layer rebuild failed.");
                    return result;
                }
                layer->SetLayerName(layers[layerIndex].layerName);
                for (const auto& effect : layers[layerIndex].effects) {
                    Effect* created = layer->AddEffect(0, effect.effectName, effect.settings, effect.palette, effect.startMs, effect.endMs, effect.selected, effect.isProtected, false);
                    if (created == nullptr) {
                        result.errorCode = std::string("INTERNAL_ERROR");
                        result.errorMessage = std::string("Effect rebuild failed.");
                        return result;
                    }
                }
            }
            _frame->GetSequenceElements().PopulateRowInformation();
            _frame->GetSequenceElements().PopulateVisibleRowInformation();
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            return result;
        });
    }

    [[nodiscard]] api::models::CompactEffectLayersResult compactEffectLayers(const api::models::CompactEffectLayersRequest& request) const {
        return RunOnMainThread<api::models::CompactEffectLayersResult>([this, request]() {
            api::models::CompactEffectLayersResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) return result;
            result.sequenceOpen = true;
            Element* element = _frame->GetSequenceElements().GetElement(request.elementName);
            if (element == nullptr) return result;
            result.elementFound = true;
            for (int layerIndex = static_cast<int>(element->GetEffectLayerCount()) - 1; layerIndex >= 0; --layerIndex) {
                if (element->GetEffectLayerCount() <= 1) break;
                EffectLayer* layer = element->GetEffectLayer(layerIndex);
                if (layer != nullptr && layer->GetEffectCount() == 0) {
                    element->RemoveEffectLayer(layerIndex);
                    result.removedLayerNumbers.push_back(layerIndex);
                }
            }
            _frame->GetSequenceElements().PopulateRowInformation();
            _frame->GetSequenceElements().PopulateVisibleRowInformation();
            if (!result.removedLayerNumbers.empty()) {
                _frame->MarkEffectsFileDirty();
                wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
                wxPostEvent(_frame, eventRowHeaderChanged);
            }
            result.layerCount = static_cast<int>(element->GetEffectLayerCount());
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
                    effect->GetID(),
                    static_cast<int>(layerIndex),
                    effect->GetEffectName(),
                    effect->GetStartTimeMS(),
                    effect->GetEndTimeMS(),
                    effect->GetSettingsAsString(),
                    effect->GetPaletteAsString()
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
