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
#include <cstdlib>
#include <cctype>
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "FSEQFile.h"
#include "AudioManager.h"
#include "media/ChordDetector.h"
#include "media/OnsetDetector.h"
#include "media/TempoDetector.h"
#include "SequenceFile.h"
#include "xLightsMain.h"
#include "diagnostics/CheckSequenceReport.h"
#include "diagnostics/SequenceChecker.h"
#include "layout/ModelPreview.h"
#include "models/DisplayAsType.h"
#include "models/ModelGroup.h"
#include "models/OutputModelManager.h"
#include "models/SubModel.h"
#include "ExternalHooks.h"
#include "effects/RenderableEffect.h"
#include "DesignerDiagnostics.h"
#include "DesignerApiRuntime.h"
#include "DesignerLaunchPolicy.h"
#include "api/models/DataLayerModels.h"
#include "api/models/EffectModels.h"
#include "api/models/ElementModels.h"
#include "api/models/LayoutModels.h"
#include "api/models/MediaModels.h"
#include "api/models/SequenceModels.h"
#include "api/models/TimingModels.h"
#include "api/transport/ErrorCatalog.h"

namespace xLightsDesigner {

namespace detail {
// Low-level xLights adapters and conversion helpers. Public handlers should
// stay in api/handlers; this namespace is only for host-facing implementation.
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

inline wxDateTime ReadDesignerApiFileModifiedTime(const std::string& path) {
    if (path.empty()) {
        return wxDateTime();
    }
    const wxFileName fileName(wxString::FromUTF8(path));
    if (!fileName.FileExists()) {
        return wxDateTime();
    }
    return fileName.GetModificationTime();
}

inline bool DesignerApiFileIsOlderThan(const std::string& candidatePath, const std::string& referencePath) {
    const wxDateTime candidate = ReadDesignerApiFileModifiedTime(candidatePath);
    const wxDateTime reference = ReadDesignerApiFileModifiedTime(referencePath);
    return candidate.IsValid() && reference.IsValid() && candidate.IsEarlierThan(reference);
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

inline std::string TrimDesignerApiToken(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    return value;
}

inline std::vector<std::string> MaterializeDesignerSubmodelLine(const Model* parent, const std::string& line) {
    std::vector<std::string> nodeIds;
    if (parent == nullptr) {
        return nodeIds;
    }
    std::stringstream stream(line);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = TrimDesignerApiToken(token);
        if (token.empty()) {
            continue;
        }
        const auto dash = token.find('-');
        int start = 0;
        int end = 0;
        if (dash == std::string::npos) {
            start = end = std::atoi(token.c_str());
        } else {
            start = std::atoi(token.substr(0, dash).c_str());
            end = std::atoi(token.substr(dash + 1).c_str());
        }
        if (start <= 0 || end <= 0) {
            continue;
        }
        const int step = start <= end ? 1 : -1;
        for (int nodeId = start;; nodeId += step) {
            if (nodeId <= static_cast<int>(parent->GetNodeCount())) {
                nodeIds.push_back(std::to_string(nodeId));
            }
            if (nodeId == end) {
                break;
            }
        }
    }
    return nodeIds;
}

inline std::string BuildDesignerSequenceRevisionToken(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return std::string();
    }
    const std::string path = frame->CurrentSeqXmlFile->GetFullPath();
    return path.empty() ? std::string() : path + "#" + std::to_string(frame->GetSequenceElements().GetChangeCount());
}

inline api::models::SequenceSummary ReadDesignerOpenSequenceSummary(xLightsFrame* frame) {
    api::models::SequenceSummary summary;
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return summary;
    }

    summary.isOpen = true;
    summary.path = frame->CurrentSeqXmlFile->GetFullPath();
    summary.revisionToken = BuildDesignerSequenceRevisionToken(frame);
    return summary;
}

inline wxString BuildDesignerRenderedFseqPath(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return wxString();
    }

    wxFileName output(frame->CurrentSeqXmlFile->GetFullPath());
    output.SetExt("fseq");
    return output.GetFullPath();
}

inline std::string ResolveDesignerRenderedFseqPath(xLightsFrame* frame) {
    if (frame == nullptr || frame->CurrentSeqXmlFile == nullptr) {
        return std::string();
    }
    return BuildDesignerRenderedFseqPath(frame).ToStdString();
}

inline api::models::FseqFileSummary ReadDesignerFseqSummary(const std::string& path) {
    api::models::FseqFileSummary summary;
    summary.path = path;
    if (path.empty()) {
        return summary;
    }
    const wxFileName fileName(wxString::FromUTF8(path));
    summary.exists = fileName.FileExists();
    summary.modifiedAt = ReadDesignerApiFileModifiedAt(fileName);
    if (!summary.exists) {
        return summary;
    }

    std::unique_ptr<FSEQFile> file(FSEQFile::openFSEQFile(path));
    if (!file) {
        return summary;
    }
    summary.readable = true;
    summary.versionMajor = file->getVersionMajor();
    summary.versionMinor = file->getVersionMinor();
    summary.frameMs = static_cast<int>(file->getStepTime());
    summary.frameCount = static_cast<int>(file->getNumFrames());
    summary.channelCount = static_cast<int>(file->getChannelCount());
    summary.maxChannel = static_cast<int>(file->getMaxChannel());
    return summary;
}

inline std::string ComputeDesignerFinalFseqFreshness(xLightsFrame* frame,
                                                     const api::models::SequenceFinalFseqState& state) {
    if (!state.exists) {
        return "missing";
    }
    if (!state.readable || !state.fseq.has_value()) {
        return "unreadable";
    }
    if (state.fseq->frameCount <= 0 || state.fseq->frameMs <= 0 || state.fseq->maxChannel <= 0) {
        return "invalid";
    }
    if (frame != nullptr && frame->CurrentSeqXmlFile != nullptr) {
        const std::string sequencePath = frame->CurrentSeqXmlFile->GetFullPath();
        if (DesignerApiFileIsOlderThan(state.finalFseqPath, sequencePath)) {
            return "stale-sequence-newer";
        }
    }
    return "current";
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

inline std::string SequenceCheckIssueType(CheckSequenceReport::ReportIssue::Type type) {
    switch (type) {
        case CheckSequenceReport::ReportIssue::CRITICAL:
            return "critical";
        case CheckSequenceReport::ReportIssue::WARNING:
            return "warning";
        case CheckSequenceReport::ReportIssue::INFO:
        default:
            return "info";
    }
}

class DesignerSequenceCheckCallbacks final : public SequenceCheckerCallbacks {
public:
    explicit DesignerSequenceCheckCallbacks(xLightsFrame* frame)
        : _frame(frame) {}

    bool IsCheckOptionDisabled(const std::string& option) const override {
        return xLightsFrame::IsCheckSequenceOptionDisabledS(option);
    }

    std::string GetRenderCacheMode() const override {
        return _frame == nullptr ? "Enabled" : _frame->EnableRenderCache().ToStdString();
    }

private:
    xLightsFrame* _frame = nullptr;
};

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
        {"suppressedDialogCount", xLightsDesigner::GetSuppressedDialogsJson().size()},
        {"suppressedDialogs", xLightsDesigner::GetSuppressedDialogsJson()},
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

inline bool IsExistingFileReadableByDesignerApi(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || std::filesystem::is_directory(path, ec)) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    return input.good();
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

inline std::string BuildStableRevisionToken(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return std::to_string(hash);
}

inline std::string BuildTimingTrackRevisionToken(TimingElement* track) {
    if (track == nullptr) {
        return std::string();
    }

    std::ostringstream stream;
    stream << track->GetName() << '|' << track->GetSubType() << '|' << (track->IsFixedTiming() ? "fixed" : "variable");
    for (size_t layerIndex = 0; layerIndex < track->GetEffectLayerCount(); ++layerIndex) {
        EffectLayer* layer = track->GetEffectLayer(static_cast<int>(layerIndex));
        if (layer == nullptr) {
            continue;
        }
        stream << "|layer:" << layerIndex << ':' << layer->GetEffectCount();
        for (auto* effect : layer->GetEffects()) {
            if (effect == nullptr) {
                continue;
            }
            stream << '|'
                   << effect->GetStartTimeMS() << '-'
                   << effect->GetEndTimeMS() << ':'
                   << effect->GetEffectName();
        }
    }
    return BuildStableRevisionToken(stream.str());
}

inline bool NativeEffectIsXldOwned(const Effect* effect) {
    return effect != nullptr && !effect->GetSettings().Get("XLD_OWNER", "").empty();
}

inline Element* ResolveNativeEffectElement(SequenceElements& sequenceElements, const std::string& elementName, const std::string& submodelName) {
    Element* element = sequenceElements.GetElement(elementName);
    if (element == nullptr || submodelName.empty()) {
        return element;
    }
    auto* modelElement = dynamic_cast<ModelElement*>(element);
    if (modelElement == nullptr) {
        return nullptr;
    }
    return modelElement->GetSubModel(submodelName);
}

inline std::string BuildNativeEffectApiId(const std::string& elementName, const std::string& submodelName, int layerIndex, const Effect* effect) {
    if (effect == nullptr) {
        return std::string();
    }
    std::ostringstream stream;
    stream << elementName << '|' << submodelName << '|' << layerIndex << '|' << effect->GetID();
    return stream.str();
}

inline std::string AddNativeEffectOwnershipSettings(const std::string& settings, const std::string& owner, const std::string& xldId) {
    SettingsMap map;
    map.Parse(nullptr, settings, "");
    map["XLD_OWNER"] = owner.empty() ? "xLightsDesigner" : owner;
    if (!xldId.empty()) {
        map["XLD_ID"] = xldId;
    }
    return map.AsString();
}

inline api::models::NativeEffectSummary BuildNativeEffectSummary(const std::string& elementName, const std::string& submodelName, int layerIndex, const Effect* effect, bool includeSettings = true) {
    api::models::NativeEffectSummary summary;
    if (effect == nullptr) {
        return summary;
    }
    summary.id = BuildNativeEffectApiId(elementName, submodelName, layerIndex, effect);
    summary.elementName = elementName;
    summary.submodelName = submodelName;
    summary.layerIndex = layerIndex;
    summary.effectIndex = effect->GetEffectIndex();
    summary.nativeId = effect->GetID();
    summary.effectName = effect->GetEffectName();
    summary.startMs = effect->GetStartTimeMS();
    summary.endMs = effect->GetEndTimeMS();
    if (includeSettings) {
        summary.settings = effect->GetSettingsAsString();
        summary.palette = effect->GetPaletteAsString();
    }
    summary.protectedEffect = effect->GetProtected();
    summary.locked = effect->IsLocked();
    summary.renderDisabled = effect->IsRenderDisabled();
    summary.xldOwner = effect->GetSettings().Get("XLD_OWNER", "");
    summary.xldId = effect->GetSettings().Get("XLD_ID", "");
    summary.xldOwned = !summary.xldOwner.empty();
    return summary;
}

inline api::models::NativeEffectLayerSummary BuildNativeEffectLayerSummary(const std::string& elementName, const std::string& submodelName, int layerIndex, const EffectLayer* layer) {
    api::models::NativeEffectLayerSummary summary;
    if (layer == nullptr) {
        return summary;
    }
    summary.elementName = elementName;
    summary.submodelName = submodelName;
    summary.layerIndex = layerIndex;
    summary.layerNumber = layer->GetLayerNumber();
    summary.layerName = layer->GetLayerName();
    summary.effectCount = layer->GetEffectCount();
    for (auto* effect : layer->GetEffects()) {
        if (NativeEffectIsXldOwned(effect)) {
            summary.hasXldOwnedEffects = true;
        } else {
            summary.hasUserOwnedEffects = true;
        }
    }
    return summary;
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

    // Owned API sequence automation should be prompt-free. Suppress
    // batch-render prompts while the open runs.
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

// DesignerApiHost is the only layer that directly touches xLightsFrame and
// xLights model/sequence objects. It returns plain API models so the transport
// and handler layers stay independent of xLights UI classes.
class DesignerApiHost {
public:
    explicit DesignerApiHost(xLightsFrame* frame)
        : _frame(frame) {}

    // Sequence state and lifecycle.
    [[nodiscard]] api::models::SequenceSummary readOpenSequence() const {
        return detail::ReadDesignerOpenSequenceSummary(_frame);
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

    // Layout and preview settings are shared by layout endpoints and sequence
    // sync checks, so they live before sequence mutation methods.
    [[nodiscard]] api::models::LayoutSettingsSummary readLayoutSettings() const {
        api::models::LayoutSettingsSummary settings;
        if (_frame == nullptr) {
            return settings;
        }

        settings.hasUnsavedRgbEffectsChanges = _frame->UnsavedRgbEffectsChanges;
        settings.hasUnsavedNetworkChanges = _frame->UnsavedNetworkChanges;
        settings.hasUnsavedLayoutChanges = settings.hasUnsavedRgbEffectsChanges || settings.hasUnsavedNetworkChanges;
        settings.modelsChangeCount = _frame->modelsChangeCount;
        settings.previewWidth = _frame->AllModels.GetPreviewWidth();
        settings.previewHeight = _frame->AllModels.GetPreviewHeight();
        settings.virtualCanvasWidth = settings.previewWidth;
        settings.virtualCanvasHeight = settings.previewHeight;
        if (auto* preview = _frame->GetLayoutPreview(); preview != nullptr) {
            preview->GetVirtualCanvasSize(settings.virtualCanvasWidth, settings.virtualCanvasHeight);
            settings.preview3d = preview->Is3D();
            settings.previewZoom = static_cast<double>(preview->GetZoom());
        }
        settings.display2dCenter0 = _frame->GetDisplay2DCenter0();
        settings.backgroundImage = _frame->GetDefaultPreviewBackgroundImage();
        settings.backgroundImageAvailable = !settings.backgroundImage.empty() && wxFileExists(wxString::FromUTF8(settings.backgroundImage.c_str()));
        settings.backgroundScaled = _frame->GetDefaultPreviewBackgroundScaled();
        settings.backgroundBrightness = _frame->GetDefaultPreviewBackgroundBrightness();
        settings.backgroundAlpha = _frame->GetDefaultPreviewBackgroundAlpha();
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

    // Sequence open/create operations deliberately suppress interactive xLights
    // prompts so noninteractive automation can prepare a sequence predictably.
    [[nodiscard]] api::models::SequenceOpenResult openSequence(const api::models::SequenceOpenRequest& request) const {
        api::models::SequenceOpenResult result;
        result.requestedPath = request.file;
        xLightsDesigner::AppendDesignerDiagnostic(std::string("sequence.open requested file=") + request.file +
                                                  " force=" + (request.force ? "1" : "0"));
        if (_frame == nullptr || request.file.empty()) {
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open rejected missingFrameOrFile");
            return result;
        }

        if (!IsDesignerApiStartupSettled()) {
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open rejected appNotReady");
            result.errorCode = "APP_NOT_READY";
            result.errorMessage = "xLightsDesigner sequence.open is blocked until xLights startup has fully settled.";
            result.retryAfterMs = GetDesignerApiStartupSettleRemainingMs();
            return result;
        }

        const auto current = readOpenSequence();
        if (!request.force && current.isOpen && current.path.has_value() && current.path.value() == request.file) {
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open skipped alreadyOpen");
            result.opened = true;
            result.sequence = current;
            return result;
        }

        if (wxIsMainThread()) {
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open executing on main thread");
            const auto guard = detail::EnterOwnedSequenceOpenState(_frame, request.file, request.force);
            if (!detail::ObtainOwnedApiAccessToPath(request.file, false)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open failed sequenceAccessDenied");
                result.errorCode = "SEQUENCE_ACCESS_DENIED";
                result.errorMessage = "Unable to obtain access to the requested sequence file.";
                return result;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(_frame, request.file)) {
                detail::ExitOwnedSequenceOpenState(_frame, guard);
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open failed showDirectoryFailed");
                result.errorCode = "SHOW_DIRECTORY_FAILED";
                result.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                return result;
            }
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open calling xLights OpenSequence on main thread");
            _frame->OpenSequence(wxString::FromUTF8(request.file), nullptr);
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open xLights OpenSequence returned on main thread");
            detail::ExitOwnedSequenceOpenState(_frame, guard);
            const auto opened = readOpenSequence();
            if (opened.isOpen && opened.path.has_value() && opened.path.value() == request.file) {
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open completed opened=1");
                result.opened = true;
                result.sequence = opened;
                return result;
            }
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open failed sequenceOpenFailed after main thread return");
            result.errorCode = "SEQUENCE_OPEN_FAILED";
            result.errorMessage = "xLights did not report the requested sequence as open after OpenSequence completed.";
            return result;
        }

        auto promise = std::make_shared<std::promise<api::models::SequenceOpenResult>>();
        auto future = promise->get_future();
        xLightsFrame* frame = _frame;
        const std::string requestedFile = request.file;
        const bool force = request.force;
        frame->CallAfter([promise, frame, requestedFile, force]() mutable {
            xLightsDesigner::AppendDesignerDiagnostic(std::string("sequence.open main-thread callback started file=") + requestedFile);
            api::models::SequenceOpenResult callbackResult;
            callbackResult.requestedPath = requestedFile;
            const auto guard = detail::EnterOwnedSequenceOpenState(frame, requestedFile, force);
            if (!detail::ObtainOwnedApiAccessToPath(requestedFile, false)) {
                detail::ExitOwnedSequenceOpenState(frame, guard);
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback failed sequenceAccessDenied");
                callbackResult.errorCode = "SEQUENCE_ACCESS_DENIED";
                callbackResult.errorMessage = "Unable to obtain access to the requested sequence file.";
                promise->set_value(callbackResult);
                return;
            }
            if (!detail::PrepareDesignerShowDirectoryForSequence(frame, requestedFile)) {
                detail::ExitOwnedSequenceOpenState(frame, guard);
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback failed showDirectoryFailed");
                callbackResult.errorCode = "SHOW_DIRECTORY_FAILED";
                callbackResult.errorMessage = "Unable to switch xLights to the target show directory before opening the sequence.";
                promise->set_value(callbackResult);
                return;
            }
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback calling xLights OpenSequence");
            frame->OpenSequence(wxString::FromUTF8(requestedFile), nullptr);
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback xLights OpenSequence returned");
            detail::ExitOwnedSequenceOpenState(frame, guard);
            const auto opened = detail::ReadDesignerOpenSequenceSummary(frame);
            if (opened.isOpen && opened.path.has_value() && opened.path.value() == requestedFile) {
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback completed opened=1");
                callbackResult.opened = true;
                callbackResult.sequence = opened;
            } else {
                xLightsDesigner::AppendDesignerDiagnostic("sequence.open callback failed sequenceOpenFailed after return");
                callbackResult.errorCode = "SEQUENCE_OPEN_FAILED";
                callbackResult.errorMessage = "xLights did not report the requested sequence as open after OpenSequence completed.";
            }
            promise->set_value(callbackResult);
        });
        const int openWaitMs = detail::ReadDesignerApiEnvIntMs("XLIGHTS_DESIGNER_SEQUENCE_OPEN_WAIT_MS", 600000);
        if (future.wait_for(std::chrono::milliseconds(openWaitMs)) != std::future_status::ready) {
            xLightsDesigner::AppendDesignerDiagnostic("sequence.open timed out waiting for main-thread callback");
            result.errorCode = "SEQUENCE_OPEN_TIMEOUT";
            result.errorMessage = "Timed out waiting for xLights to finish opening the requested sequence.";
            return result;
        }
        xLightsDesigner::AppendDesignerDiagnostic("sequence.open worker received callback result");
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

    [[nodiscard]] api::models::SequenceFocusResult focusSequence() const {
        return RunOnMainThread<api::models::SequenceFocusResult>([this]() {
            api::models::SequenceFocusResult result;
            result.sequence = readOpenSequence();
            if (_frame == nullptr) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "xLights frame is unavailable.";
                return result;
            }
            if (_frame->CurrentSeqXmlFile == nullptr || !result.sequence.isOpen) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }

            const int sequencerPage = _frame->Notebook1->GetPageIndex(_frame->PanelSequencer);
            if (sequencerPage != wxNOT_FOUND) {
                _frame->Notebook1->SetSelection(sequencerPage);
            }
            _frame->EnableSequenceControls(true);
            result.focused = true;
            result.sequence = readOpenSequence();
            return result;
        });
    }

    // Sequence persistence, rendering, and validation. These methods preserve
    // xLights' normal file formats while giving XLD explicit status evidence.
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

    [[nodiscard]] api::models::SequenceCheckResult checkSequence() const {
        return RunOnMainThread<api::models::SequenceCheckResult>([this]() {
            api::models::SequenceCheckResult result;
            result.sequence = readOpenSequence();
            result.sequenceOpen = result.sequence.isOpen;
            if (_frame == nullptr) {
                result.errorCode = "VALIDATION_ERROR";
                result.errorMessage = "xLights frame is unavailable.";
                return result;
            }

            _frame->RecalcModels();

            CheckSequenceReport report;
            for (const auto& section : CheckSequenceReport::REPORT_SECTIONS) {
                report.AddSection(section);
            }
            report.SetShowFolder(_frame->GetShowDirectory());
            if (_frame->CurrentSeqXmlFile != nullptr) {
                wxFileName sequenceFile(_frame->CurrentSeqXmlFile->GetFullPath());
                sequenceFile.SetExt("xsq");
                report.SetSequencePath(sequenceFile.GetFullPath().ToStdString());
            }

            detail::DesignerSequenceCheckCallbacks callbacks(_frame);
            SequenceChecker checker(
                _frame->GetSequenceElements(),
                _frame->AllModels,
                *_frame->GetOutputManager(),
                _frame->CurrentSeqXmlFile,
                _frame->GetShowDirectory(),
                &callbacks);
            checker.RunFullCheck(report);

            result.checked = true;
            result.errorCount = report.GetTotalErrors();
            result.warningCount = report.GetTotalWarnings();
            result.showDirectory = report.GetShowFolder();
            result.sequencePath = report.GetSequencePath();
            result.generatedAt = report.GetGeneratedTime();
            for (const auto& reportSection : report.GetSections()) {
                api::models::SequenceCheckSection section;
                section.id = reportSection.id;
                section.title = reportSection.title;
                section.description = reportSection.description;
                section.errorCount = reportSection.errorCount;
                section.warningCount = reportSection.warningCount;
                section.issues.reserve(reportSection.issues.size());
                for (const auto& reportIssue : reportSection.issues) {
                    api::models::SequenceCheckIssue issue;
                    issue.type = detail::SequenceCheckIssueType(reportIssue.type);
                    issue.message = reportIssue.message;
                    issue.category = reportIssue.category;
                    issue.modelName = reportIssue.modelName;
                    issue.effectName = reportIssue.effectName;
                    issue.startTimeMs = reportIssue.startTimeMS;
                    issue.layerIndex = reportIssue.layerIndex;
                    section.issues.push_back(std::move(issue));
                }
                result.sections.push_back(std::move(section));
            }
            return result;
        });
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

            const bool exported = _frame->ExportVideoPreview(outputFile.GetFullPath(), false, request.width, request.height);
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

    // Render inspection endpoints expose final FSEQ evidence for app-side
    // review, sync checks, and generated DataLayer validation.
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

    [[nodiscard]] api::models::SequenceFinalFseqState readFinalFseqState() const {
        api::models::SequenceFinalFseqState state;
        const auto sequence = readOpenSequence();
        state.sequenceOpen = sequence.isOpen;
        state.sequencePath = sequence.path.value_or("");
        state.revisionToken = sequence.revisionToken.value_or("");
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr || !sequence.isOpen) {
            return state;
        }

        state.finalFseqPath = detail::ResolveDesignerRenderedFseqPath(_frame);
        if (state.finalFseqPath.empty()) {
            state.finalFseqPath = detail::BuildDesignerRenderedFseqPath(_frame).ToStdString();
        }
        state.fseq = detail::ReadDesignerFseqSummary(state.finalFseqPath);
        state.exists = state.fseq->exists;
        state.readable = state.fseq->readable;
        state.freshness = detail::ComputeDesignerFinalFseqFreshness(_frame, state);
        return state;
    }

    [[nodiscard]] api::models::SequenceSyncHealthSummary readSyncHealth() const {
        api::models::SequenceSyncHealthSummary health;
        const auto sequence = readOpenSequence();
        health.sequenceOpen = sequence.isOpen;
        health.sequencePath = sequence.path.value_or("");
        health.revisionToken = sequence.revisionToken.value_or("");
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr || !sequence.isOpen) {
            health.status = "blocked";
            health.warnings.push_back("No sequence is open.");
            return health;
        }

        health.dataLayers = readDataLayers();
        health.finalFseq = readFinalFseqState();
        health.status = "ready";
        bool hasXldLayer = false;
        bool hasBlockedIssue = false;
        for (const auto& layer : health.dataLayers.layers) {
            if (layer.isXldLayer) {
                hasXldLayer = true;
                if (!layer.pathExists) {
                    health.status = "update-required";
                    health.warnings.push_back("An XLD DataLayer references a missing generated FSEQ file.");
                }
                if (layer.xldManifest.has_value()) {
                    for (const auto& warning : layer.xldManifest->warnings) {
                        health.warnings.push_back(warning);
                    }
                    if (layer.xldManifest->status == "blocked") {
                        hasBlockedIssue = true;
                    }
                }
                if (health.finalFseq.exists && detail::DesignerApiFileIsOlderThan(health.finalFseq.finalFseqPath, layer.dataSource)) {
                    health.status = "update-required";
                    health.finalFseq.freshness = "stale-xld-layer-newer";
                    health.warnings.push_back("The final controller FSEQ is older than the generated XLD DataLayer input.");
                }
            }
        }
        if (hasXldLayer && !health.finalFseq.exists) {
            health.status = "update-required";
            health.warnings.push_back("The final controller FSEQ is missing after XLD DataLayer binding.");
        }
        if (hasXldLayer && health.finalFseq.exists && health.finalFseq.freshness != "current") {
            health.status = "update-required";
            health.warnings.push_back("The final controller FSEQ freshness is " + health.finalFseq.freshness + ".");
        }
        if (_frame->mSavedChangeCount != _frame->GetSequenceElements().GetChangeCount()) {
            health.status = "update-required";
            health.warnings.push_back("The xLights sequence has unsaved changes.");
        }
        if (hasBlockedIssue) {
            health.status = "blocked";
        }
        return health;
    }

    // DataLayer operations attach generated XLD FSEQ files to the sequence and
    // validate manifest/layout alignment before final render.
    [[nodiscard]] api::models::DataLayerListSummary readDataLayers() const {
        api::models::DataLayerListSummary summary;
        const auto sequence = readOpenSequence();
        summary.sequenceOpen = sequence.isOpen;
        summary.sequencePath = sequence.path.value_or("");
        summary.revisionToken = sequence.revisionToken.value_or("");
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr || !summary.sequenceOpen) {
            return summary;
        }

        DataLayerSet& layers = _frame->CurrentSeqXmlFile->GetDataLayers();
        const int nutcrackerIndex = findNutcrackerIndex(layers);
        const int count = layers.GetNumLayers();
        const std::string currentChannelMapFingerprint = readLayoutChannelMap().fingerprint;
        summary.layers.reserve(std::max(0, count));
        for (int index = 0; index < count; ++index) {
            if (auto* layer = layers.GetDataLayer(static_cast<size_t>(index)); layer != nullptr) {
                summary.layers.push_back(makeDataLayerSummary(*layer, index, nutcrackerIndex, currentChannelMapFingerprint, summary.sequencePath));
            }
        }
        return summary;
    }

    [[nodiscard]] api::models::DataLayerMutationResult upsertDataLayer(const api::models::DataLayerUpsertRequest& request) const {
        return RunOnMainThread<api::models::DataLayerMutationResult>([this, request]() {
            api::models::DataLayerMutationResult result;
            result.sequenceOpen = _frame != nullptr && _frame->CurrentSeqXmlFile != nullptr;
            result.sequencePath = result.sequenceOpen ? _frame->CurrentSeqXmlFile->GetFullPath() : std::string();
            if (!result.sequenceOpen) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }
            if (_frame->IsReadOnlyMode()) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "DataLayers cannot be changed in read-only mode.";
                return result;
            }
            if (request.name.empty() || request.sourceFseqPath.empty()) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "DataLayer name and source FSEQ path are required.";
                return result;
            }
            if (isNutcrackerName(request.name)) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "The Nutcracker render layer cannot be replaced.";
                return result;
            }
            if (!detail::ObtainOwnedApiAccessToPath(request.sourceFseqPath, false) ||
                !wxFileExists(wxString::FromUTF8(request.sourceFseqPath))) {
                result.errorCode = "DATA_LAYER_FILE_INACCESSIBLE";
                result.errorMessage = "The DataLayer source FSEQ file is inaccessible.";
                return result;
            }

            DataLayerSet& layers = _frame->CurrentSeqXmlFile->GetDataLayers();
            int layerIndex = findDataLayerIndex(layers, request.name);
            DataLayer* layer = layerIndex >= 0 ? layers.GetDataLayer(static_cast<size_t>(layerIndex)) : nullptr;
            if (layer == nullptr) {
                layer = layers.AddDataLayer(request.name, request.sourceFseqPath, request.dataSourcePath.empty() ? request.sourceFseqPath : request.dataSourcePath);
                layerIndex = layers.GetNumLayers() - 1;
            }
            layer->SetName(request.name);
            layer->SetSource(request.sourceFseqPath);
            layer->SetDataSource(request.dataSourcePath.empty() ? request.sourceFseqPath : request.dataSourcePath);
            if (request.channelCount > 0) {
                layer->SetNumChannels(request.channelCount);
            }
            if (request.frameCount > 0) {
                layer->SetNumFrames(request.frameCount);
            }
            layer->SetChannelOffset(std::max(0, request.channelOffset));

            moveDataLayerForPlacement(layers, layerIndex, request.placement);
            _frame->GetSequenceElements().IncrementChangeCount(nullptr);

            result.ok = true;
            result.layers = readDataLayers();
            const int updatedIndex = findDataLayerIndex(layers, request.name);
            result.layer = updatedIndex >= 0
                ? makeDataLayerSummary(*layers.GetDataLayer(static_cast<size_t>(updatedIndex)), updatedIndex, findNutcrackerIndex(layers), readLayoutChannelMap().fingerprint, result.sequencePath)
                : api::models::DataLayerSummary{};
            return result;
        });
    }

    [[nodiscard]] api::models::DataLayerMutationResult removeDataLayer(const api::models::DataLayerRemoveRequest& request) const {
        return RunOnMainThread<api::models::DataLayerMutationResult>([this, request]() {
            api::models::DataLayerMutationResult result;
            result.sequenceOpen = _frame != nullptr && _frame->CurrentSeqXmlFile != nullptr;
            result.sequencePath = result.sequenceOpen ? _frame->CurrentSeqXmlFile->GetFullPath() : std::string();
            if (!result.sequenceOpen) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }
            DataLayerSet& layers = _frame->CurrentSeqXmlFile->GetDataLayers();
            const int layerIndex = resolveDataLayerIndex(layers, request.name, request.index);
            if (layerIndex < 0) {
                result.errorCode = "DATA_LAYER_NOT_FOUND";
                result.errorMessage = "DataLayer was not found.";
                return result;
            }
            DataLayer* layer = layers.GetDataLayer(static_cast<size_t>(layerIndex));
            if (layer == nullptr || isNutcrackerName(layer->GetName())) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "The Nutcracker render layer cannot be removed.";
                return result;
            }
            layers.RemoveDataLayer(layerIndex);
            _frame->GetSequenceElements().IncrementChangeCount(nullptr);
            result.ok = true;
            result.layers = readDataLayers();
            return result;
        });
    }

    [[nodiscard]] api::models::DataLayerMutationResult reorderDataLayer(const api::models::DataLayerReorderRequest& request) const {
        return RunOnMainThread<api::models::DataLayerMutationResult>([this, request]() {
            api::models::DataLayerMutationResult result;
            result.sequenceOpen = _frame != nullptr && _frame->CurrentSeqXmlFile != nullptr;
            result.sequencePath = result.sequenceOpen ? _frame->CurrentSeqXmlFile->GetFullPath() : std::string();
            if (!result.sequenceOpen) {
                result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
                result.errorMessage = "No sequence is open.";
                return result;
            }
            DataLayerSet& layers = _frame->CurrentSeqXmlFile->GetDataLayers();
            int layerIndex = resolveDataLayerIndex(layers, request.name, request.index);
            if (layerIndex < 0) {
                result.errorCode = "DATA_LAYER_NOT_FOUND";
                result.errorMessage = "DataLayer was not found.";
                return result;
            }
            DataLayer* layer = layers.GetDataLayer(static_cast<size_t>(layerIndex));
            if (layer == nullptr || isNutcrackerName(layer->GetName())) {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "The Nutcracker render layer cannot be reordered.";
                return result;
            }
            if (!request.placement.empty()) {
                moveDataLayerForPlacement(layers, layerIndex, request.placement);
            } else if (request.targetIndex >= 0) {
                moveDataLayerToIndex(layers, layerIndex, std::min(request.targetIndex, layers.GetNumLayers() - 1));
            } else {
                result.errorCode = std::string(api::transport::errors::ValidationError);
                result.errorMessage = "A targetIndex or placement is required.";
                return result;
            }
            _frame->GetSequenceElements().IncrementChangeCount(nullptr);
            result.ok = true;
            result.layers = readDataLayers();
            const int updatedIndex = resolveDataLayerIndex(layers, request.name, request.index);
            result.layer = updatedIndex >= 0
                ? makeDataLayerSummary(*layers.GetDataLayer(static_cast<size_t>(updatedIndex)), updatedIndex, findNutcrackerIndex(layers), readLayoutChannelMap().fingerprint, result.sequencePath)
                : api::models::DataLayerSummary{};
            return result;
        });
    }

    [[nodiscard]] api::models::DataLayerValidationResult validateDataLayer(const api::models::DataLayerValidateRequest& request) const {
        api::models::DataLayerValidationResult result;
        result.sequenceOpen = _frame != nullptr && _frame->CurrentSeqXmlFile != nullptr;
        if (!result.sequenceOpen) {
            result.errorCode = std::string(api::transport::errors::SequenceNotOpen);
            result.errorMessage = "No sequence is open.";
            return result;
        }

        DataLayerSet& layers = _frame->CurrentSeqXmlFile->GetDataLayers();
        const int layerIndex = resolveDataLayerIndex(layers, request.name, request.index);
        const int nutcrackerIndex = findNutcrackerIndex(layers);
        if (layerIndex >= 0) {
            if (auto* layer = layers.GetDataLayer(static_cast<size_t>(layerIndex)); layer != nullptr) {
                result.layer = makeDataLayerSummary(*layer, layerIndex, nutcrackerIndex, readLayoutChannelMap().fingerprint, _frame->CurrentSeqXmlFile->GetFullPath());
                result.path = result.layer.dataSource;
            }
        }
        if (!request.fseqPath.empty()) {
            result.path = request.fseqPath;
        }
        result.fseq = detail::ReadDesignerFseqSummary(result.path);
        result.fileExists = result.fseq.exists;
        result.readable = result.fseq.readable;
        result.supportedVersion = result.fseq.readable && result.fseq.versionMajor > 0;
        if (!result.fileExists) {
            result.errorCode = "DATA_LAYER_FILE_MISSING";
            result.errorMessage = "The DataLayer FSEQ file does not exist.";
            return result;
        }
        if (!result.readable || !result.supportedVersion) {
            result.errorCode = "UNSUPPORTED_FSEQ";
            result.errorMessage = "The DataLayer FSEQ file could not be read by xLights.";
            return result;
        }
        if (request.expectedChannelCount > 0 && result.fseq.channelCount != request.expectedChannelCount) {
            result.warnings.push_back("FSEQ channel count does not match the expected channel count.");
        }
        if (request.expectedFrameCount > 0 && result.fseq.frameCount != request.expectedFrameCount) {
            result.warnings.push_back("FSEQ frame count does not match the expected frame count.");
        }
        if (request.expectedFrameMs > 0 && result.fseq.frameMs != request.expectedFrameMs) {
            result.warnings.push_back("FSEQ frame timing does not match the expected frame timing.");
        }
        if (result.layer.xldManifest.has_value()) {
            for (const auto& warning : result.layer.xldManifest->warnings) {
                result.warnings.push_back(warning);
            }
        }
        result.ok = true;
        return result;
    }

    // Media and show-folder state.
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
        wxFileName mediaName(_frame->CurrentSeqXmlFile->GetMediaFile());
        if (!mediaName.GetFullPath().empty() && !mediaName.IsAbsolute() && !_frame->CurrentDir.empty()) {
            mediaName.MakeAbsolute(_frame->CurrentDir);
        }
        summary.mediaFile = mediaName.GetFullPath().ToStdString();
        summary.durationMs = static_cast<int>(_frame->CurrentSeqXmlFile->GetSequenceDurationMS());
        if (auto* media = _frame->CurrentSeqXmlFile->GetMedia(); media != nullptr && media->IsOk()) {
            summary.durationMs = static_cast<int>(media->LengthMS());
            summary.sampleRate = static_cast<int>(media->GetRate());
            summary.channelCount = media->GetChannels();
        }
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

    [[nodiscard]] api::models::MediaPathAccessValidationResult validateMediaPathAccess(const api::models::MediaPathAccessValidationRequest& request) const {
        api::models::MediaPathAccessValidationResult result;
        for (const auto& requested : request.checks) {
            api::models::MediaPathAccessCheckResult check;
            check.kind = requested.kind;
            check.path = requested.path;
            check.trustedRoot = detail::IsOwnedTrustedRootPath(requested.path);

            std::error_code ec;
            const auto resolvedPath = detail::ResolveOwnedAccessTarget(requested.path);
            check.exists = !resolvedPath.empty() && std::filesystem::exists(resolvedPath, ec);
            if (_frame != nullptr && !_frame->CurrentDir.empty() && !resolvedPath.empty()) {
                const auto showRoot = detail::ResolveOwnedAccessTarget(_frame->CurrentDir.ToStdString());
                check.withinCurrentShowDirectory = !showRoot.empty() && detail::IsPathWithinRoot(resolvedPath, showRoot);
            }

            if (requested.path.empty()) {
                check.errorCode = "VALIDATION_ERROR";
                check.errorMessage = "Path is required.";
            } else if (requested.mustExist && !check.exists) {
                check.errorCode = "PATH_NOT_FOUND";
                check.errorMessage = "Path does not exist.";
            }

            if (check.exists) {
                if (std::filesystem::is_directory(resolvedPath, ec)) {
                    check.readable = std::filesystem::exists(resolvedPath, ec);
                } else {
                    std::ifstream input(resolvedPath, std::ios::binary);
                    check.readable = input.good();
                }
            }

            if (check.exists) {
                const auto probeDir = std::filesystem::is_directory(resolvedPath, ec) ? resolvedPath : resolvedPath.parent_path();
                if (!probeDir.empty() && std::filesystem::exists(probeDir, ec)) {
                    const auto probe = probeDir / ".xld-media-access-test";
                    std::ofstream output(probe.string(), std::ios::out | std::ios::trunc);
                    check.writable = output.is_open();
                    if (output.is_open()) {
                        output.close();
                        std::filesystem::remove(probe, ec);
                    }
                }
            }

            if (!check.trustedRoot) {
                check.warnings.push_back("Path is not inside an xLightsDesigner trusted development root.");
            } else {
                check.warnings.push_back("Trusted development root status does not guarantee xLights can read the file; readable/writable checks report actual access.");
            }

            if (!check.errorCode.has_value() && requested.requireReadable && !check.readable) {
                check.errorCode = "PATH_NOT_READABLE";
                check.errorMessage = "Path exists but could not be read.";
            }
            if (!check.errorCode.has_value() && requested.requireWritable && !check.writable) {
                check.errorCode = "PATH_NOT_WRITABLE";
                check.errorMessage = "Path exists but could not be written.";
            }

            check.accessible = !check.errorCode.has_value() &&
                (!requested.mustExist || check.exists) &&
                (!requested.requireReadable || check.readable) &&
                (!requested.requireWritable || check.writable);
            if (!check.accessible) {
                result.ok = false;
            }
            result.checks.push_back(std::move(check));
        }
        return result;
    }

    [[nodiscard]] api::models::MediaAudioCapabilitiesSummary readAudioCapabilities() const {
        api::models::MediaAudioCapabilitiesSummary summary;
        if (_frame == nullptr) {
            summary.warnings.push_back("xLights frame is not available.");
            return summary;
        }

        if (_frame->CurrentSeqXmlFile != nullptr) {
            summary.sequenceOpen = true;
            const auto mediaFile = _frame->CurrentSeqXmlFile->GetMediaFile();
            if (!mediaFile.empty()) {
                wxFileName mediaName(mediaFile);
                if (!mediaName.IsAbsolute() && !_frame->CurrentDir.empty()) {
                    mediaName.MakeAbsolute(_frame->CurrentDir);
                }
                summary.mediaFile = mediaName.GetFullPath().ToStdString();
                summary.mediaAvailable = mediaName.FileExists();
                if (!summary.mediaAvailable) {
                    summary.warnings.push_back("The current sequence references media that xLights could not find.");
                }
            } else {
                summary.warnings.push_back("The current sequence does not reference a media file.");
            }
        } else {
            summary.warnings.push_back("No sequence is open.");
        }

        const bool mediaLoaded = _frame->CurrentSeqXmlFile != nullptr &&
            _frame->CurrentSeqXmlFile->GetMedia() != nullptr &&
            _frame->CurrentSeqXmlFile->GetMedia()->IsOk();
        summary.capabilities.push_back({"mediaReference", summary.mediaAvailable, "xLights", summary.mediaAvailable ? "Current sequence media file is available." : "Current sequence media file is not available."});
        summary.capabilities.push_back({"waveform", false, "xLights", "Owned API waveform extraction is not implemented in this contract slice."});
        summary.capabilities.push_back({"timingTracks", true, "xLights", "Timing tracks are exposed through the timing API."});
        summary.capabilities.push_back({"audioOnsets", mediaLoaded, "xLights", mediaLoaded ? "Spectral-flux onset detection is available for the current audio." : "Current sequence audio is not loaded."});
        summary.capabilities.push_back({"audioTempo", mediaLoaded, "xLights", mediaLoaded ? "Autocorrelation tempo detection is available for the current audio." : "Current sequence audio is not loaded."});
        summary.capabilities.push_back({"audioChords", mediaLoaded, "xLights", mediaLoaded ? "Chromagram chord/key detection is available for the current audio." : "Current sequence audio is not loaded."});
        summary.capabilities.push_back({"audioAnalysisHandoff", mediaLoaded, "xLightsDesigner", mediaLoaded ? "Owned API can return normalized xLights audio-analysis tracks." : "Open a sequence with loaded audio before running analysis."});
        return summary;
    }

    [[nodiscard]] api::models::MediaAudioAnalysisSummary analyzeAudio() const {
        api::models::MediaAudioAnalysisSummary summary;
        if (_frame == nullptr) {
            summary.warnings.push_back("xLights frame is not available.");
            return summary;
        }
        if (_frame->CurrentSeqXmlFile == nullptr) {
            summary.warnings.push_back("No sequence is open.");
            return summary;
        }

        summary.sequenceOpen = true;
        summary.sequencePath = _frame->CurrentSeqXmlFile->GetFullPath();
        wxFileName mediaName(_frame->CurrentSeqXmlFile->GetMediaFile());
        if (!mediaName.GetFullPath().empty() && !mediaName.IsAbsolute() && !_frame->CurrentDir.empty()) {
            mediaName.MakeAbsolute(_frame->CurrentDir);
        }
        summary.mediaFile = mediaName.GetFullPath().ToStdString();
        summary.mediaAvailable = mediaName.FileExists();
        summary.durationMs = static_cast<int>(_frame->CurrentSeqXmlFile->GetSequenceDurationMS());

        AudioManager* media = _frame->CurrentSeqXmlFile->GetMedia();
        if (media == nullptr || !media->IsOk()) {
            summary.mediaAvailable = false;
            summary.warnings.push_back("The current sequence audio is not loaded.");
            return summary;
        }

        const int lengthMs = static_cast<int>(std::max<long>(0, media->LengthMS()));
        summary.mediaAvailable = true;
        summary.mediaHash = media->Hash();
        summary.durationMs = lengthMs;
        summary.sampleRate = static_cast<int>(media->GetRate());
        summary.channelCount = media->GetChannels();

        std::vector<long> onsets = DetectOnsets(media);
        if (onsets.empty()) {
            summary.warnings.push_back("xLights Audio Onsets did not find usable onset marks.");
        } else {
            api::models::MediaAudioAnalysisTrack track;
            track.trackName = "xLights Audio Onsets";
            track.trackType = "onsets";
            track.description = "xLights spectral-flux onset detection. Track name is a display label only; the description defines its meaning.";
            track.timingGranularity = "percussive-onset-region";
            track.confidence = 0.72;
            for (size_t i = 0; i < onsets.size(); i++) {
                const long start = std::clamp<long>(onsets[i], 0, lengthMs);
                const long end = (i + 1 < onsets.size()) ? std::clamp<long>(onsets[i + 1], 0, lengthMs) : lengthMs;
                if (end <= start) {
                    continue;
                }
                track.marks.push_back({
                    static_cast<int>(start),
                    static_cast<int>(end),
                    std::to_string(i + 1),
                    0.72,
                    "onset"
                });
            }
            summary.timingTracks.push_back(std::move(track));
        }

        TempoResult tempo = DetectTempo(media);
        if (tempo.beatMS.empty() || tempo.bpm <= 0) {
            summary.warnings.push_back("xLights Audio Tempo did not find a usable beat grid.");
        } else {
            api::models::MediaAudioAnalysisTrack track;
            track.trackName = wxString::Format("xLights Audio Tempo %.1f BPM", tempo.bpm).ToStdString();
            track.trackType = "tempo";
            track.description = "xLights tempo detection from autocorrelation of the onset envelope. Marks are beat-to-beat regions.";
            track.timingGranularity = "beat-region";
            track.confidence = tempo.confidence;
            for (size_t i = 0; i < tempo.beatMS.size(); i++) {
                const long start = std::clamp<long>(tempo.beatMS[i], 0, lengthMs);
                const long end = (i + 1 < tempo.beatMS.size()) ? std::clamp<long>(tempo.beatMS[i + 1], 0, lengthMs) : lengthMs;
                if (end <= start) {
                    continue;
                }
                track.marks.push_back({
                    static_cast<int>(start),
                    static_cast<int>(end),
                    std::to_string(i + 1),
                    tempo.confidence,
                    "beat"
                });
            }
            summary.timingTracks.push_back(std::move(track));
            summary.evidence.push_back({
                "tempo",
                "xLights detected tempo using onset-envelope autocorrelation.",
                tempo.confidence,
                {
                    {"bpm", wxString::Format("%.2f", tempo.bpm).ToStdString()},
                    {"beatCount", std::to_string(tempo.beatMS.size())}
                }
            });
        }

        HarmonyAnalysis harmony = DetectChords(media);
        if (harmony.chords.empty()) {
            summary.warnings.push_back("xLights Audio Chords did not find usable chord segments.");
        } else {
            api::models::MediaAudioAnalysisTrack track;
            track.trackName = harmony.key.empty() ? "xLights Audio Chords" : std::string("xLights Audio Chords in ") + harmony.key;
            track.trackType = "chords";
            track.description = "xLights chromagram chord and key detection. Labels are detected chord names for harmonic color and mood planning.";
            track.timingGranularity = "chord-segment";
            track.confidence = 0.68;
            for (const auto& chord : harmony.chords) {
                const long start = std::clamp<long>(chord.startMS, 0, lengthMs);
                const long end = std::clamp<long>(chord.endMS, 0, lengthMs);
                if (end <= start) {
                    continue;
                }
                track.marks.push_back({
                    static_cast<int>(start),
                    static_cast<int>(end),
                    chord.name,
                    0.68,
                    "chord"
                });
            }
            summary.timingTracks.push_back(std::move(track));
            summary.evidence.push_back({
                "harmony",
                "xLights detected key and chord segments using chromagram analysis.",
                0.68,
                {
                    {"detectedKey", harmony.key.empty() ? "unknown" : harmony.key},
                    {"chordSegmentCount", std::to_string(harmony.chords.size())}
                }
            });
        }

        summary.evidence.push_back({
            "mediaIdentity",
            "Current xLights sequence media identity used for audio-analysis provenance.",
            0.9,
            {
                {"durationMs", std::to_string(lengthMs)},
                {"sampleRate", std::to_string(media->GetRate())},
                {"channelCount", std::to_string(media->GetChannels())},
                {"mediaHash", summary.mediaHash.value_or("")}
            }
        });
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

        std::error_code effectsPathError;
        const auto rgbEffectsPath = std::filesystem::path(request.showDirectory) / XLIGHTS_RGBEFFECTS_FILE;
        if (std::filesystem::exists(rgbEffectsPath, effectsPathError) &&
            !detail::IsExistingFileReadableByDesignerApi(rgbEffectsPath)) {
            result.errorCode = "SHOW_DIRECTORY_LAYOUT_UNREADABLE";
            result.errorMessage = "The show directory layout file exists but xLightsDesigner could not read it.";
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

        const bool accessOk = detail::ObtainOwnedApiAccessToPath(request.showDirectory, true);
        const bool trustedNoninteractiveShowDir =
            xLightsDesigner::IsNonInteractiveLaunch() && detail::IsOwnedTrustedRootPath(request.showDirectory);
        if (!accessOk && !trustedNoninteractiveShowDir) {
            result.errorCode = "SHOW_DIRECTORY_ACCESS_DENIED";
            result.errorMessage = "Unable to obtain write access to the requested show directory.";
            return result;
        }
        if (!accessOk && trustedNoninteractiveShowDir) {
            xLightsDesigner::AppendDesignerDiagnostic(std::string("setShowDirectory using noninteractive trusted root without write probe showDirectory=") +
                                                      request.showDirectory);
        }

        xLightsFrame* frame = _frame;
        auto switchOnMainThread = [request, targetShowDir, frame]() {
            api::models::MediaShowDirectoryResult callbackResult;
            callbackResult.previousShowDirectory = frame != nullptr ? frame->CurrentDir.ToStdString() : std::string();
            callbackResult.showDirectory = request.showDirectory;
            if (frame == nullptr) {
                callbackResult.errorCode = "VALIDATION_ERROR";
                callbackResult.errorMessage = "xLights frame is not available.";
                return callbackResult;
            }

            const bool hadSequence = frame->CurrentSeqXmlFile != nullptr;
            const bool previousRenderMode = frame->_renderMode;
            const bool previousPromptBatchRenderIssues = frame->_promptBatchRenderIssues;
            if (request.force) {
                frame->_renderMode = true;
                frame->_promptBatchRenderIssues = false;
                if (frame->CurrentSeqXmlFile != nullptr) {
                    frame->mSavedChangeCount = frame->GetSequenceElements().GetChangeCount();
                }
                frame->UnsavedRgbEffectsChanges = false;
                frame->UnsavedNetworkChanges = false;
            }

            const bool switched = frame->SetDir(targetShowDir, request.permanent);

            if (request.force) {
                frame->_renderMode = previousRenderMode;
                frame->_promptBatchRenderIssues = previousPromptBatchRenderIssues;
            }

            callbackResult.sequenceClosed = hadSequence && frame->CurrentSeqXmlFile == nullptr;
            callbackResult.showDirectory = frame->CurrentDir.ToStdString();
            callbackResult.changed = switched && frame->CurrentDir == targetShowDir;
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

        xLightsFrame* frame = _frame;
        auto promptOnMainThread = [request, targetShowDir, frame]() {
            api::models::MediaShowDirectoryResult callbackResult;
            callbackResult.previousShowDirectory = frame != nullptr ? frame->CurrentDir.ToStdString() : std::string();
            callbackResult.showDirectory = request.showDirectory;
            if (frame == nullptr) {
                callbackResult.errorCode = "VALIDATION_ERROR";
                callbackResult.errorMessage = "xLights frame is not available.";
                return callbackResult;
            }

            wxDirDialog dialog(frame,
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
            const bool hadSequence = frame->CurrentSeqXmlFile != nullptr;
            const bool previousRenderMode = frame->_renderMode;
            const bool previousPromptBatchRenderIssues = frame->_promptBatchRenderIssues;
            if (request.force) {
                frame->_renderMode = true;
                frame->_promptBatchRenderIssues = false;
                if (frame->CurrentSeqXmlFile != nullptr) {
                    frame->mSavedChangeCount = frame->GetSequenceElements().GetChangeCount();
                }
                frame->UnsavedRgbEffectsChanges = false;
                frame->UnsavedNetworkChanges = false;
            }

            const bool switched = frame->SetDir(targetShowDir, request.permanent);

            if (request.force) {
                frame->_renderMode = previousRenderMode;
                frame->_promptBatchRenderIssues = previousPromptBatchRenderIssues;
            }

            callbackResult.sequenceClosed = hadSequence && frame->CurrentSeqXmlFile == nullptr;
            callbackResult.showDirectory = frame->CurrentDir.ToStdString();
            callbackResult.changed = switched && frame->CurrentDir == targetShowDir;
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

    // Read-only layout discovery. These endpoints provide the physical display
    // skeleton for native-effect planning, proof validation, and optional
    // advanced direct-channel writing.
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
            const auto rotation = location.GetRotation();
            const auto scale = location.GetScaleMatrix();
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
            modelSummary.rotationX = static_cast<double>(rotation.x);
            modelSummary.rotationY = static_cast<double>(rotation.y);
            modelSummary.rotationZ = static_cast<double>(rotation.z);
            modelSummary.scaleX = static_cast<double>(scale.x);
            modelSummary.scaleY = static_cast<double>(scale.y);
            modelSummary.scaleZ = static_cast<double>(scale.z);
            modelSummary.renderWidth = static_cast<double>(location.GetRenderWi());
            modelSummary.renderHeight = static_cast<double>(location.GetRenderHt());
            modelSummary.renderDepth = static_cast<double>(location.GetRenderDp());
            modelSummary.supportedRenderStyles = model->GetBufferStyles();
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
            std::unordered_map<int, std::string> parentNodeIdByActChan;
            parentNodeIdByActChan.reserve(model->GetNodeCount());
            for (uint32_t parentNodeIndex = 0; parentNodeIndex < model->GetNodeCount(); ++parentNodeIndex) {
                const auto* parentNode = model->GetNode(parentNodeIndex);
                if (parentNode != nullptr) {
                    parentNodeIdByActChan.emplace(parentNode->ActChan, std::to_string(parentNodeIndex + 1));
                }
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
                row.supportedRenderStyles = submodel->GetBufferStyles();
                row.startChannel = static_cast<int>(submodel->GetFirstChannel()) + 1;
                row.endChannel = static_cast<int>(submodel->GetLastChannel()) + 1;
                row.nodeCount = static_cast<int>(submodel->GetNodeCount());
                row.vertical = submodel->IsVertical();
                row.ranges = submodel->IsRanges();
                row.nodeRows.reserve(static_cast<size_t>(submodel->GetNumRanges()));
                for (int rangeIndex = 0; rangeIndex < submodel->GetNumRanges(); ++rangeIndex) {
                    api::models::LayoutSubmodelRowSummary nodeRow;
                    nodeRow.order = rangeIndex;
                    nodeRow.rowId = "row-" + std::to_string(rangeIndex + 1);
                    nodeRow.label = "Row " + std::to_string(rangeIndex + 1);
                    nodeRow.rawLine = submodel->GetRange(rangeIndex);
                    nodeRow.nodeIds = detail::MaterializeDesignerSubmodelLine(model, nodeRow.rawLine);
                    if (!nodeRow.nodeIds.empty()) {
                        row.nodeRows.push_back(std::move(nodeRow));
                    }
                }
                row.nodeIds.reserve(submodel->GetNodeCount());
                for (uint32_t submodelNodeIndex = 0; submodelNodeIndex < submodel->GetNodeCount(); ++submodelNodeIndex) {
                    const auto* submodelNode = submodel->GetNode(submodelNodeIndex);
                    if (submodelNode == nullptr) {
                        continue;
                    }
                    auto parentNodeId = parentNodeIdByActChan.find(submodelNode->ActChan);
                    if (parentNodeId != parentNodeIdByActChan.end()) {
                        row.nodeIds.push_back(parentNodeId->second);
                    }
                }
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

    [[nodiscard]] api::models::LayoutRenderBufferNodesSummary readLayoutRenderBufferNodes(const api::models::LayoutRenderBufferNodesRequest& request) const {
        api::models::LayoutRenderBufferNodesSummary summary;
        summary.targetName = request.targetName;
        summary.requestedRenderStyle = request.renderStyle.empty() ? "Default" : request.renderStyle;
        summary.camera = request.camera.empty() ? "2D" : request.camera;
        summary.transform = request.transform.empty() ? "None" : request.transform;
        summary.stagger = request.stagger;
        summary.deep = request.deep;
        if (_frame == nullptr || request.targetName.empty()) {
            return summary;
        }

        auto* target = _frame->AllModels.GetModel(request.targetName);
        if (target == nullptr) {
            return summary;
        }

        summary.found = true;
        summary.targetName = target->GetFullName();
        summary.supportedRenderStyles = target->GetBufferStyles();
        summary.adjustedRenderStyle = target->AdjustBufferStyle(summary.requestedRenderStyle);
        if (summary.adjustedRenderStyle != summary.requestedRenderStyle) {
            summary.warnings.push_back("Requested render style was adjusted by xLights for this target.");
        }

        std::vector<NodeBaseClassPtr> renderNodes;
        target->InitRenderBufferNodes(summary.adjustedRenderStyle,
                                      summary.camera,
                                      summary.transform,
                                      renderNodes,
                                      summary.bufferWidth,
                                      summary.bufferHeight,
                                      summary.stagger,
                                      summary.deep);
        if (renderNodes.empty()) {
            summary.warnings.push_back("xLights materialized no render-buffer nodes for this target/style.");
            return summary;
        }

        auto resolveNode = [this, target](const NodeBaseClass* node, size_t fallbackIndex) {
            struct ResolvedNode {
                const Model* model = nullptr;
                uint32_t index = 0;
                bool matched = false;
            };
            ResolvedNode result;
            if (node == nullptr || _frame == nullptr) {
                return result;
            }
            auto matchesNode = [node](const NodeBaseClass* candidate) {
                return candidate != nullptr
                    && candidate->ActChan == node->ActChan
                    && candidate->GetChanCount() == node->GetChanCount();
            };
            auto findInModel = [&](const Model* model) {
                if (model == nullptr) {
                    return false;
                }
                for (uint32_t nodeIndex = 0; nodeIndex < model->GetNodeCount(); ++nodeIndex) {
                    if (matchesNode(model->GetNode(nodeIndex))) {
                        result.model = model;
                        result.index = nodeIndex;
                        result.matched = true;
                        return true;
                    }
                }
                return false;
            };

            if (findInModel(node->model)) {
                return result;
            }
            if (findInModel(target)) {
                return result;
            }
            for (auto it = _frame->AllModels.begin(); it != _frame->AllModels.end(); ++it) {
                if (findInModel(it->second)) {
                    return result;
                }
            }
            result.index = static_cast<uint32_t>(fallbackIndex);
            return result;
        };

        summary.nodes.reserve(renderNodes.size());
        int unmatchedNodes = 0;
        for (size_t renderNodeIndex = 0; renderNodeIndex < renderNodes.size(); ++renderNodeIndex) {
            const auto* node = renderNodes[renderNodeIndex].get();
            if (node == nullptr) {
                continue;
            }

            const auto resolved = resolveNode(node, renderNodeIndex);
            api::models::LayoutRenderBufferNodeSummary row;
            row.nodeId = static_cast<int>(resolved.index + 1);
            row.nodeIndex = static_cast<int>(resolved.index);
            row.stringIndex = static_cast<int>(node->StringNum);
            row.channelStartZeroBased = static_cast<int>(node->ActChan);
            row.channelStart = row.channelStartZeroBased + 1;
            row.channelCount = static_cast<int>(node->GetChanCount());
            if (resolved.model != nullptr) {
                row.parentModelName = resolved.model->GetName();
                row.name = resolved.model->GetNodeName(resolved.index, true);
            } else {
                row.parentModelName = target->GetName();
                if (node->name != nullptr) {
                    row.name = *node->name;
                }
                ++unmatchedNodes;
            }
            for (uint32_t coordIndex = 0; coordIndex < node->Coords.size(); ++coordIndex) {
                const auto& coord = node->Coords[coordIndex];
                api::models::LayoutNodeCoordSummary coordRow;
                coordRow.bufferX = coord.bufX;
                coordRow.bufferY = coord.bufY;
                row.coords.push_back(std::move(coordRow));
            }
            summary.nodes.push_back(std::move(row));
        }

        if (unmatchedNodes > 0) {
            summary.warnings.push_back("Some render-buffer nodes could not be matched back to a source model by channel.");
        }
        return summary;
    }

    [[nodiscard]] api::models::LayoutChannelMapSummary readLayoutChannelMap() const {
        api::models::LayoutChannelMapSummary summary;
        summary.mappingEvidence = "xlights-calculated-node-act-channel";
        if (_frame == nullptr) {
            summary.warnings.push_back("xLights frame is unavailable.");
            return summary;
        }

        std::ostringstream fingerprint;
        // Keep this fingerprint tied to channel-map content only.  Volatile
        // layout dirty counters can change after sequence/data-layer edits and
        // would incorrectly mark an otherwise aligned XLD FSEQ as stale.
        fingerprint << "models=" << _frame->AllModels.size() << ";";
        int maxChannel = 0;
        bool hasWarnings = false;
        for (auto it = _frame->AllModels.begin(); it != _frame->AllModels.end(); ++it) {
            auto* model = it->second;
            if (model == nullptr) {
                continue;
            }

            api::models::LayoutChannelMapTarget target;
            target.targetName = model->GetName();
            target.displayAs = DisplayAsTypeToString(model->GetDisplayAs());
            target.startChannel = static_cast<int>(model->GetFirstChannel()) + 1;
            target.endChannel = static_cast<int>(model->GetLastChannel()) + 1;
            target.nodeCount = static_cast<int>(model->GetNodeCount());
            target.usableForFseq = model->GetDisplayAs() != DisplayAsType::ModelGroup && target.nodeCount > 0;
            fingerprint << target.targetName << ":" << target.startChannel << "-" << target.endChannel << ":" << target.nodeCount << ";";

            if (model->GetDisplayAs() == DisplayAsType::ModelGroup) {
                target.warnings.push_back("Model groups do not expose direct node channel mapping; use member model mappings.");
                hasWarnings = true;
            }

            target.nodes.reserve(std::max(0, target.nodeCount));
            for (uint32_t nodeIndex = 0; nodeIndex < model->GetNodeCount(); ++nodeIndex) {
                auto* node = model->GetNode(nodeIndex);
                if (node == nullptr) {
                    continue;
                }
                const int channelCount = static_cast<int>(node->GetChanCount());
                api::models::LayoutChannelMapNode row;
                row.nodeId = static_cast<int>(nodeIndex + 1);
                row.nodeIndex = static_cast<int>(nodeIndex);
                row.stringIndex = static_cast<int>(node->StringNum);
                row.name = model->GetNodeName(nodeIndex, true);
                row.channelStartZeroBased = static_cast<int>(node->ActChan);
                row.channelStart = row.channelStartZeroBased + 1;
                row.channelCount = channelCount;
                row.evidence = "NodeBaseClass.ActChan/GetChanCount";
                if (channelCount <= 0) {
                    target.warnings.push_back("A node has no channel count.");
                    hasWarnings = true;
                }
                maxChannel = std::max(maxChannel, row.channelStartZeroBased + channelCount);
                target.nodes.push_back(std::move(row));
            }
            summary.targets.push_back(std::move(target));
        }
        summary.maxChannelCount = maxChannel;
        summary.usableForFseq = maxChannel > 0 && !summary.targets.empty();
        if (hasWarnings) {
            summary.warnings.push_back("Some display elements require group-member expansion or have incomplete node channel evidence.");
        }
        summary.fingerprint = std::to_string(std::hash<std::string>{}(fingerprint.str()));
        summary.snapshotId = "xlights-channel-map-" + summary.fingerprint;
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

    // Sequence element inventory and row ordering.
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
            if (auto* modelElement = dynamic_cast<ModelElement*>(element); modelElement != nullptr) {
                elementSummary.selected = modelElement->GetSelected();
            }
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

    [[nodiscard]] api::models::SelectedDisplayElementsSummary readSelectedDisplayElements() const {
        api::models::SelectedDisplayElementsSummary summary;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return summary;
        }

        summary.sequenceOpen = true;
        auto& sequenceElements = _frame->GetSequenceElements();
        for (size_t i = 0; i < sequenceElements.GetElementCount(); ++i) {
            auto* element = dynamic_cast<ModelElement*>(sequenceElements.GetElement(i));
            if (element != nullptr && element->GetSelected()) {
                summary.selectedElementNames.push_back(element->GetName());
            }
        }
        return summary;
    }

    [[nodiscard]] api::models::SetSelectedDisplayElementsResult setSelectedDisplayElements(const api::models::SetSelectedDisplayElementsRequest& request) const {
        return RunOnMainThread<api::models::SetSelectedDisplayElementsResult>([this, request]() {
            api::models::SetSelectedDisplayElementsResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            auto& sequenceElements = _frame->GetSequenceElements();

            std::vector<ModelElement*> requested;
            for (const auto& name : request.elementNames) {
                auto* element = dynamic_cast<ModelElement*>(sequenceElements.GetElement(name));
                if (element == nullptr) {
                    result.missingNames.push_back(name);
                    continue;
                }
                requested.push_back(element);
            }
            if (!result.missingNames.empty()) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Selection contains unknown display elements.");
                return result;
            }

            if (request.replaceExisting) {
                sequenceElements.UnSelectAllElements();
            }
            for (auto* element : requested) {
                element->SetSelected(true);
            }
            result.ok = true;
            result.selectedCount = static_cast<int>(requested.size());
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            return result;
        });
    }

    // Native effect and layer operations.
    [[nodiscard]] api::models::NativeEffectSchemaResult readNativeEffectSchemas(const api::models::NativeEffectSchemaRequest& request) const {
        api::models::NativeEffectSchemaResult result;
        if (_frame == nullptr) {
            return result;
        }

        std::set<std::string> requestedNames(request.effectNames.begin(), request.effectNames.end());
        auto& effectManager = _frame->GetEffectManager();
        for (int index = 0; index < static_cast<int>(effectManager.size()); ++index) {
            RenderableEffect* effect = effectManager.GetEffect(index);
            if (effect == nullptr) {
                continue;
            }
            const std::string effectName = effect->Name();
            if (!requestedNames.empty() && requestedNames.find(effectName) == requestedNames.end()) {
                continue;
            }

            api::models::NativeEffectSchema schema;
            schema.effectName = effectName;
            schema.rawMetadata = effect->GetMetadata();
            if (schema.rawMetadata.is_object()) {
                schema.canvasMode = schema.rawMetadata.value("canvasMode", false);
                if (schema.rawMetadata.contains("properties") && schema.rawMetadata["properties"].is_array()) {
                    schema.properties = schema.rawMetadata["properties"];
                }
                if (schema.rawMetadata.contains("groups") && schema.rawMetadata["groups"].is_array()) {
                    schema.groups = schema.rawMetadata["groups"];
                }
                if (schema.rawMetadata.contains("visibilityRules") && schema.rawMetadata["visibilityRules"].is_array()) {
                    schema.visibilityRules = schema.rawMetadata["visibilityRules"];
                }
            }
            result.schemas.push_back(std::move(schema));
        }
        result.revisionToken = std::string("effect-schemas:") + std::to_string(effectManager.size());
        return result;
    }

    [[nodiscard]] api::models::NativeEffectListResult readNativeEffects(const api::models::NativeEffectListRequest& request) const {
        api::models::NativeEffectListResult result;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return result;
        }

        result.sequenceOpen = true;
        auto& sequenceElements = _frame->GetSequenceElements();
        auto collectEffects = [&](const std::string& parentName, const std::string& submodelName, Element* element) {
            if (element == nullptr) {
                return;
            }
            result.elementFound = true;
            for (size_t layerIndex = 0; layerIndex < element->GetEffectLayerCount(); ++layerIndex) {
                EffectLayer* layer = element->GetEffectLayer(static_cast<int>(layerIndex));
                if (layer == nullptr) {
                    continue;
                }
                for (auto* effect : layer->GetEffects()) {
                    if (effect == nullptr) {
                        continue;
                    }
                    if (request.startMs.has_value() && effect->GetEndTimeMS() < *request.startMs) {
                        continue;
                    }
                    if (request.endMs.has_value() && effect->GetStartTimeMS() > *request.endMs) {
                        continue;
                    }
                    if (request.xldOnly && !detail::NativeEffectIsXldOwned(effect)) {
                        continue;
                    }
                    result.effects.push_back(detail::BuildNativeEffectSummary(parentName, submodelName, static_cast<int>(layerIndex), effect, request.includeSettings));
                }
            }
        };

        const size_t elementCount = request.elementName.empty() ? sequenceElements.GetElementCount() : 1;
        for (size_t elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Element* element = request.elementName.empty()
                ? sequenceElements.GetElement(elementIndex)
                : sequenceElements.GetElement(request.elementName);
            if (element == nullptr) {
                continue;
            }
            if (!request.submodelName.empty()) {
                collectEffects(element->GetName(), request.submodelName, detail::ResolveNativeEffectElement(sequenceElements, element->GetName(), request.submodelName));
            } else {
                collectEffects(element->GetName(), std::string(), element);
                if (request.elementName.empty()) {
                    auto* modelElement = dynamic_cast<ModelElement*>(element);
                    if (modelElement != nullptr) {
                        for (int submodelIndex = 0; submodelIndex < modelElement->GetSubModelAndStrandCount(); ++submodelIndex) {
                            SubModelElement* submodel = modelElement->GetSubModel(submodelIndex);
                            if (submodel != nullptr) {
                                collectEffects(element->GetName(), submodel->GetName(), submodel);
                            }
                        }
                    }
                }
            }
            if (!request.elementName.empty()) {
                break;
            }
        }
        return result;
    }

    [[nodiscard]] api::models::NativeEffectLayerListResult readNativeEffectLayers(const api::models::NativeEffectLayerListRequest& request) const {
        api::models::NativeEffectLayerListResult result;
        if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
            return result;
        }

        result.sequenceOpen = true;
        auto& sequenceElements = _frame->GetSequenceElements();
        auto collectLayers = [&](const std::string& parentName, const std::string& submodelName, Element* element) {
            if (element == nullptr) {
                return;
            }
            result.elementFound = true;
            for (size_t layerIndex = 0; layerIndex < element->GetEffectLayerCount(); ++layerIndex) {
                result.layers.push_back(detail::BuildNativeEffectLayerSummary(parentName, submodelName, static_cast<int>(layerIndex), element->GetEffectLayer(static_cast<int>(layerIndex))));
            }
        };

        const size_t elementCount = request.elementName.empty() ? sequenceElements.GetElementCount() : 1;
        for (size_t elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Element* element = request.elementName.empty()
                ? sequenceElements.GetElement(elementIndex)
                : sequenceElements.GetElement(request.elementName);
            if (element == nullptr) {
                continue;
            }
            if (!request.submodelName.empty()) {
                collectLayers(element->GetName(), request.submodelName, detail::ResolveNativeEffectElement(sequenceElements, element->GetName(), request.submodelName));
            } else {
                collectLayers(element->GetName(), std::string(), element);
                if (request.elementName.empty()) {
                    auto* modelElement = dynamic_cast<ModelElement*>(element);
                    if (modelElement != nullptr) {
                        for (int submodelIndex = 0; submodelIndex < modelElement->GetSubModelAndStrandCount(); ++submodelIndex) {
                            SubModelElement* submodel = modelElement->GetSubModel(submodelIndex);
                            if (submodel != nullptr) {
                                collectLayers(element->GetName(), submodel->GetName(), submodel);
                            }
                        }
                    }
                }
            }
            if (!request.elementName.empty()) {
                break;
            }
        }
        return result;
    }

    [[nodiscard]] api::models::NativeEffectLayerMutationResult ensureNativeEffectLayer(const api::models::NativeEffectLayerEnsureRequest& request) const {
        return RunOnMainThread<api::models::NativeEffectLayerMutationResult>([this, request]() {
            api::models::NativeEffectLayerMutationResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            Element* element = detail::ResolveNativeEffectElement(_frame->GetSequenceElements(), request.elementName, request.submodelName);
            if (element == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested sequence element or submodel was not found.");
                return result;
            }
            result.elementFound = true;

            int targetIndex = request.layerIndex;
            if (targetIndex < 0) {
                targetIndex = static_cast<int>(element->GetEffectLayerCount());
            }
            while (static_cast<int>(element->GetEffectLayerCount()) <= targetIndex) {
                element->AddEffectLayer();
                result.created = true;
            }
            EffectLayer* layer = element->GetEffectLayer(targetIndex);
            if (layer == nullptr) {
                result.errorCode = std::string("INTERNAL_ERROR");
                result.errorMessage = std::string("Requested layer could not be created.");
                return result;
            }
            if (!request.layerName.empty()) {
                layer->SetLayerName(request.layerName);
            }
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.layerFound = true;
            result.ok = true;
            result.layer = detail::BuildNativeEffectLayerSummary(request.elementName, request.submodelName, targetIndex, layer);
            return result;
        });
    }

    [[nodiscard]] api::models::NativeEffectLayerMutationResult removeNativeEffectLayer(const api::models::NativeEffectLayerRemoveRequest& request) const {
        return RunOnMainThread<api::models::NativeEffectLayerMutationResult>([this, request]() {
            api::models::NativeEffectLayerMutationResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            Element* element = detail::ResolveNativeEffectElement(_frame->GetSequenceElements(), request.elementName, request.submodelName);
            if (element == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested sequence element or submodel was not found.");
                return result;
            }
            result.elementFound = true;
            EffectLayer* layer = element->GetEffectLayer(request.layerIndex);
            if (layer == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested effect layer was not found.");
                return result;
            }
            result.layerFound = true;
            result.layer = detail::BuildNativeEffectLayerSummary(request.elementName, request.submodelName, request.layerIndex, layer);
            if (result.layer.hasUserOwnedEffects && !request.allowUserOwned) {
                result.errorCode = std::string("OWNERSHIP_ERROR");
                result.errorMessage = std::string("Refusing to remove a layer containing user-owned native effects.");
                return result;
            }
            element->RemoveEffectLayer(request.layerIndex);
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.removed = true;
            return result;
        });
    }

    [[nodiscard]] api::models::NativeEffectMutationResult upsertNativeEffect(const api::models::NativeEffectUpsertRequest& request) const {
        return RunOnMainThread<api::models::NativeEffectMutationResult>([this, request]() {
            api::models::NativeEffectMutationResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            Element* element = detail::ResolveNativeEffectElement(_frame->GetSequenceElements(), request.elementName, request.submodelName);
            if (element == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested sequence element or submodel was not found.");
                return result;
            }
            result.elementFound = true;
            while (static_cast<int>(element->GetEffectLayerCount()) <= request.layerIndex) {
                element->AddEffectLayer();
            }
            EffectLayer* layer = element->GetEffectLayer(request.layerIndex);
            if (layer == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested effect layer was not found.");
                return result;
            }
            result.layerFound = true;

            Effect* target = request.nativeId >= 0 ? layer->GetEffectFromID(request.nativeId) : nullptr;
            if (target == nullptr && !request.xldId.empty()) {
                for (auto* effect : layer->GetEffects()) {
                    if (effect != nullptr && effect->GetSettings().Get("XLD_ID", "") == request.xldId) {
                        target = effect;
                        break;
                    }
                }
            }
            if (target != nullptr && !detail::NativeEffectIsXldOwned(target)) {
                result.errorCode = std::string("OWNERSHIP_ERROR");
                result.errorMessage = std::string("Refusing to update a user-owned native effect.");
                return result;
            }

            const std::string settings = detail::AddNativeEffectOwnershipSettings(request.settings, request.xldOwner, request.xldId);
            if (target == nullptr) {
                if (request.replaceExistingXld && !request.xldId.empty()) {
                    for (int i = layer->GetEffectCount() - 1; i >= 0; --i) {
                        Effect* effect = layer->GetEffect(i);
                        if (effect != nullptr && effect->GetSettings().Get("XLD_ID", "") == request.xldId) {
                            layer->DeleteEffect(effect->GetID());
                        }
                    }
                }
                target = layer->AddEffect(0, request.effectName, settings, request.palette, request.startMs, request.endMs, EFFECT_NOT_SELECTED, false);
                result.created = target != nullptr;
            } else {
                target->SetEffectName(request.effectName);
                target->SetEffectIndex(_frame->GetEffectManager().GetEffectIndex(request.effectName));
                target->SetStartTimeMS(request.startMs);
                target->SetEndTimeMS(request.endMs);
                target->SetSettings(settings, false);
                target->SetPalette(request.palette);
                result.updated = true;
            }

            if (target == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("xLights rejected the native effect name or timing.");
                return result;
            }
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.effect = detail::BuildNativeEffectSummary(request.elementName, request.submodelName, request.layerIndex, target, true);
            return result;
        });
    }

    [[nodiscard]] api::models::NativeEffectMutationResult removeNativeEffect(const api::models::NativeEffectRemoveRequest& request) const {
        return RunOnMainThread<api::models::NativeEffectMutationResult>([this, request]() {
            api::models::NativeEffectMutationResult result;
            if (_frame == nullptr || _frame->CurrentSeqXmlFile == nullptr) {
                return result;
            }
            result.sequenceOpen = true;
            Element* element = detail::ResolveNativeEffectElement(_frame->GetSequenceElements(), request.elementName, request.submodelName);
            if (element == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested sequence element or submodel was not found.");
                return result;
            }
            result.elementFound = true;
            EffectLayer* layer = element->GetEffectLayer(request.layerIndex);
            if (layer == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested effect layer was not found.");
                return result;
            }
            result.layerFound = true;

            Effect* target = request.nativeId >= 0 ? layer->GetEffectFromID(request.nativeId) : nullptr;
            if (target == nullptr && !request.xldId.empty()) {
                for (auto* effect : layer->GetEffects()) {
                    if (effect != nullptr && effect->GetSettings().Get("XLD_ID", "") == request.xldId) {
                        target = effect;
                        break;
                    }
                }
            }
            if (target == nullptr) {
                result.errorCode = std::string("VALIDATION_ERROR");
                result.errorMessage = std::string("Requested native effect was not found.");
                return result;
            }
            if (!request.allowUserOwned && !detail::NativeEffectIsXldOwned(target)) {
                result.errorCode = std::string("OWNERSHIP_ERROR");
                result.errorMessage = std::string("Refusing to remove a user-owned native effect.");
                return result;
            }

            result.effect = detail::BuildNativeEffectSummary(request.elementName, request.submodelName, request.layerIndex, target, true);
            layer->DeleteEffect(target->GetID());
            _frame->MarkEffectsFileDirty();
            wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
            wxPostEvent(_frame, eventRowHeaderChanged);
            result.ok = true;
            result.removed = true;
            return result;
        });
    }

    // Timing track read operations.
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
        summary.revisionToken = detail::BuildTimingTrackRevisionToken(track);
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
            const int layerCount = static_cast<int>(track->GetEffectLayerCount());
            for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
                if (auto* layer = track->GetEffectLayer(layerIndex); layer != nullptr) {
                    markCount += layer->GetEffectCount();
                }
            }
            summary.tracks.push_back({
                track->GetName(),
                track->IsFixedTiming() ? "fixed" : "variable",
                track->GetSubType(),
                markCount,
                layerCount,
                detail::BuildTimingTrackRevisionToken(track)
            });
        }
        return summary;
    }

private:
    // Serialization helpers for xLights-owned DataLayer objects and XLD
    // manifest validation.
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

    [[nodiscard]] static bool isNutcrackerName(const std::string& name) {
        return detail::ToLowerCopy(name) == "nutcracker";
    }

    [[nodiscard]] static bool isXldLayerNameOrPath(const std::string& name, const std::string& path) {
        const auto lowerName = detail::ToLowerCopy(name);
        const auto lowerPath = detail::ToLowerCopy(path);
        return lowerName.rfind("xld", 0) == 0 || lowerPath.find(".xld.") != std::string::npos;
    }

    [[nodiscard]] static int findNutcrackerIndex(DataLayerSet& layers) {
        for (int index = 0; index < layers.GetNumLayers(); ++index) {
            if (auto* layer = layers.GetDataLayer(static_cast<size_t>(index)); layer != nullptr && isNutcrackerName(layer->GetName())) {
                return index;
            }
        }
        return -1;
    }

    [[nodiscard]] static int findDataLayerIndex(DataLayerSet& layers, const std::string& name) {
        if (name.empty()) {
            return -1;
        }
        for (int index = 0; index < layers.GetNumLayers(); ++index) {
            if (auto* layer = layers.GetDataLayer(static_cast<size_t>(index)); layer != nullptr && layer->GetName() == name) {
                return index;
            }
        }
        return -1;
    }

    [[nodiscard]] static int resolveDataLayerIndex(DataLayerSet& layers, const std::string& name, int requestedIndex) {
        if (!name.empty()) {
            return findDataLayerIndex(layers, name);
        }
        return requestedIndex >= 0 && requestedIndex < layers.GetNumLayers() ? requestedIndex : -1;
    }

    static void moveDataLayerToIndex(DataLayerSet& layers, int fromIndex, int targetIndex) {
        if (fromIndex < 0 || fromIndex >= layers.GetNumLayers()) {
            return;
        }
        targetIndex = std::max(0, std::min(targetIndex, layers.GetNumLayers() - 1));
        while (fromIndex > targetIndex) {
            layers.MoveLayerUp(fromIndex);
            --fromIndex;
        }
        while (fromIndex < targetIndex) {
            layers.MoveLayerDown(fromIndex);
            ++fromIndex;
        }
    }

    static void moveDataLayerForPlacement(DataLayerSet& layers, int layerIndex, const std::string& placement) {
        const auto normalized = detail::ToLowerCopy(placement);
        if (normalized.empty()) {
            return;
        }
        const int nutcrackerIndex = findNutcrackerIndex(layers);
        if (nutcrackerIndex < 0) {
            return;
        }
        if (normalized == "above-nutcracker" || normalized == "above" || normalized == "overlay") {
            moveDataLayerToIndex(layers, layerIndex, std::max(0, nutcrackerIndex));
        } else if (normalized == "below-nutcracker" || normalized == "below" || normalized == "base") {
            moveDataLayerToIndex(layers, layerIndex, std::min(layers.GetNumLayers() - 1, nutcrackerIndex + 1));
        }
    }

    [[nodiscard]] static std::string dataLayerPathMode(const std::string& path) {
        if (path.empty() || path[0] == '<') {
            return "internal";
        }
        wxFileName fileName(wxString::FromUTF8(path));
        return fileName.IsAbsolute() ? "absolute" : "relative";
    }

    [[nodiscard]] static std::string readManifestString(const nlohmann::json& manifest, const char* key) {
        if (!manifest.contains(key)) {
            return std::string();
        }
        const auto& value = manifest.at(key);
        return value.is_string() ? value.get<std::string>() : std::string();
    }

    [[nodiscard]] static int readManifestInt(const nlohmann::json& manifest, const char* key) {
        if (!manifest.contains(key)) {
            return 0;
        }
        const auto& value = manifest.at(key);
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_number()) {
            return static_cast<int>(value.get<double>());
        }
        return 0;
    }

    [[nodiscard]] static long long readManifestInt64(const nlohmann::json& manifest, const char* key) {
        if (!manifest.contains(key)) {
            return 0;
        }
        const auto& value = manifest.at(key);
        if (value.is_number_integer()) {
            return value.get<long long>();
        }
        if (value.is_number()) {
            return static_cast<long long>(value.get<double>());
        }
        return 0;
    }

    [[nodiscard]] static std::string expectedXldManifestPathForFseq(const std::string& fseqPath) {
        if (fseqPath.empty() || fseqPath[0] == '<') {
            return std::string();
        }
        wxFileName manifest(wxString::FromUTF8(fseqPath));
        if (manifest.GetFullName().empty()) {
            return std::string();
        }
        manifest.SetExt("manifest.json");
        return manifest.GetFullPath().ToStdString();
    }

    static void addXldManifestIssue(
        api::models::XldManifestValidationSummary& manifest,
        const std::string& issueCode,
        const std::string& warning,
        bool blocksFinalOutput
    ) {
        manifest.issueCodes.push_back(issueCode);
        manifest.warnings.push_back(warning);
        if (blocksFinalOutput) {
            manifest.status = "blocked";
        } else if (manifest.status != "blocked") {
            manifest.status = "warning";
        }
    }

    [[nodiscard]] static std::string normalizedPathForComparison(const std::string& path) {
        if (path.empty()) {
            return std::string();
        }
        wxFileName fileName(wxString::FromUTF8(path));
        fileName.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);
        return fileName.GetFullPath().ToStdString();
    }

    [[nodiscard]] static api::models::XldManifestValidationSummary validateXldManifestForLayer(
        const api::models::DataLayerSummary& layer,
        const std::string& currentChannelMapFingerprint,
        const std::string& currentSequencePath
    ) {
        api::models::XldManifestValidationSummary result;
        result.expected = true;
        result.path = expectedXldManifestPathForFseq(layer.dataSource);
        if (result.path.empty()) {
            result.status = "blocked";
            addXldManifestIssue(result, "xld_manifest_path_unresolved", "The XLD manifest path could not be resolved for this DataLayer.", true);
            return result;
        }

        wxFileName manifestFile(wxString::FromUTF8(result.path));
        result.exists = manifestFile.FileExists();
        if (!result.exists) {
            result.status = "blocked";
            addXldManifestIssue(result, "xld_manifest_missing", "The XLD manifest is missing for this generated DataLayer.", true);
            return result;
        }

        std::ifstream input(result.path);
        if (!input.good()) {
            result.status = "blocked";
            addXldManifestIssue(result, "xld_manifest_unreadable", "The XLD manifest exists but could not be read.", true);
            return result;
        }

        nlohmann::json manifest = nlohmann::json::parse(input, nullptr, false);
        result.readable = !manifest.is_discarded() && manifest.is_object();
        if (!result.readable) {
            result.status = "blocked";
            addXldManifestIssue(result, "xld_manifest_invalid_json", "The XLD manifest is not valid JSON.", true);
            return result;
        }

        result.status = "safe";
        result.artifactType = readManifestString(manifest, "artifactType");
        result.artifactVersion = readManifestInt(manifest, "artifactVersion");
        result.appId = readManifestString(manifest, "appId");
        result.targetSequencePath = readManifestString(manifest, "targetSequencePath");
        result.generatedFseqPath = readManifestString(manifest, "generatedFseqPath");
        result.generatedFseqBasename = readManifestString(manifest, "generatedFseqBasename");
        result.generatedFseqSizeBytes = readManifestInt64(manifest, "generatedFseqSizeBytes");
        result.displaySnapshotId = readManifestString(manifest, "displaySnapshotId");
        result.channelMapSnapshotId = readManifestString(manifest, "channelMapSnapshotId");
        result.channelMapFingerprint = readManifestString(manifest, "channelMapFingerprint");
        result.outputConfigurationFingerprint = readManifestString(manifest, "outputConfigurationFingerprint");
        result.frameMs = readManifestInt(manifest, "frameTimeMs");
        result.frameCount = readManifestInt(manifest, "frameCount");
        result.channelCount = readManifestInt(manifest, "channelCount");
        result.maxChannel = readManifestInt(manifest, "maxChannel");
        if (result.maxChannel <= 0) {
            result.maxChannel = result.channelCount;
        }
        result.dataLayerName = readManifestString(manifest, "dataLayerName");
        result.generatedAt = readManifestString(manifest, "generatedAt");

        if (result.appId != "XLD" || result.artifactType != "xld_fseq_manifest_v1") {
            addXldManifestIssue(result, "xld_manifest_invalid", "The XLD manifest is not a valid XLD AI layer manifest.", true);
        }
        if (!result.generatedFseqBasename.empty()) {
            wxFileName layerFile(wxString::FromUTF8(layer.dataSource));
            if (layerFile.GetFullName().ToStdString() != result.generatedFseqBasename) {
                addXldManifestIssue(result, "xld_manifest_fseq_path_mismatch", "The XLD manifest references a different generated FSEQ file.", true);
            }
        }
        if (!result.generatedFseqPath.empty()) {
            const auto expectedPath = normalizedPathForComparison(layer.dataSource);
            const auto manifestPath = normalizedPathForComparison(result.generatedFseqPath);
            if (!expectedPath.empty() && !manifestPath.empty() && expectedPath != manifestPath) {
                addXldManifestIssue(result, "xld_manifest_fseq_path_mismatch", "The XLD manifest generated FSEQ path does not match the DataLayer path.", true);
            }
        }
        if (!result.targetSequencePath.empty()) {
            const auto expectedPath = normalizedPathForComparison(currentSequencePath);
            const auto manifestPath = normalizedPathForComparison(result.targetSequencePath);
            if (!expectedPath.empty() && !manifestPath.empty() && expectedPath != manifestPath) {
                addXldManifestIssue(result, "xld_manifest_sequence_mismatch", "The XLD manifest was generated for a different xLights sequence.", true);
            }
        }
        if (result.generatedFseqSizeBytes > 0 && !layer.dataSource.empty()) {
            std::error_code error;
            const auto actualSize = std::filesystem::file_size(std::filesystem::path(layer.dataSource), error);
            if (!error && static_cast<long long>(actualSize) != result.generatedFseqSizeBytes) {
                addXldManifestIssue(result, "xld_manifest_fseq_size_mismatch", "The generated XLD FSEQ file size does not match the manifest.", true);
            }
        }
        if (layer.fseq.has_value() && layer.fseq->readable) {
            if (result.channelCount > 0 && layer.fseq->channelCount != result.channelCount) {
                addXldManifestIssue(result, "xld_manifest_fseq_header_mismatch", "The generated XLD FSEQ channel count does not match the manifest.", true);
            }
            if (result.maxChannel > 0 && layer.fseq->maxChannel != result.maxChannel) {
                addXldManifestIssue(result, "xld_manifest_fseq_header_mismatch", "The generated XLD FSEQ max channel does not match the manifest.", true);
            }
            if (result.frameCount > 0 && layer.fseq->frameCount != result.frameCount) {
                addXldManifestIssue(result, "xld_manifest_fseq_header_mismatch", "The generated XLD FSEQ frame count does not match the manifest.", true);
            }
            if (result.frameMs > 0 && layer.fseq->frameMs != result.frameMs) {
                addXldManifestIssue(result, "xld_manifest_fseq_header_mismatch", "The generated XLD FSEQ frame timing does not match the manifest.", true);
            }
        }
        if (result.channelMapFingerprint.empty()) {
            addXldManifestIssue(result, "xld_manifest_channel_map_missing", "The XLD manifest does not include channel-map fingerprint evidence.", true);
        } else if (currentChannelMapFingerprint.empty()) {
            addXldManifestIssue(result, "xld_manifest_channel_map_unverified", "The current xLights channel-map fingerprint could not be verified.", false);
        } else if (result.channelMapFingerprint != currentChannelMapFingerprint) {
            addXldManifestIssue(result, "xld_manifest_stale_channel_map", "The XLD manifest channel-map fingerprint does not match the current xLights layout.", true);
        }
        if (result.displaySnapshotId.empty()) {
            addXldManifestIssue(result, "xld_manifest_display_snapshot_missing", "The XLD manifest does not identify the display snapshot used for generation.", false);
        }

        return result;
    }

    [[nodiscard]] static api::models::DataLayerSummary makeDataLayerSummary(
        const DataLayer& layer,
        int index,
        int nutcrackerIndex,
        const std::string& currentChannelMapFingerprint,
        const std::string& currentSequencePath
    ) {
        api::models::DataLayerSummary summary;
        summary.index = index;
        summary.name = layer.GetName();
        summary.source = layer.GetSource();
        summary.dataSource = layer.GetDataSource();
        summary.isNutcracker = isNutcrackerName(summary.name);
        summary.isXldLayer = isXldLayerNameOrPath(summary.name, summary.dataSource);
        summary.numChannels = layer.GetNumChannels();
        summary.numFrames = layer.GetNumFrames();
        summary.channelOffset = layer.GetChannelOffset();
        summary.lorConvertParams = layer.GetLORConvertParams();
        summary.pathMode = dataLayerPathMode(summary.dataSource);
        summary.pathExists = !summary.dataSource.empty() && summary.dataSource[0] != '<' && wxFileExists(wxString::FromUTF8(summary.dataSource));
        summary.participatesInFinalRender = true;
        if (summary.isNutcracker) {
            summary.placement = "nutcracker";
        } else if (nutcrackerIndex < 0) {
            summary.placement = "unknown";
        } else if (index < nutcrackerIndex) {
            summary.placement = "above-nutcracker";
        } else {
            summary.placement = "below-nutcracker";
        }
        if (summary.pathExists) {
            summary.fseq = detail::ReadDesignerFseqSummary(summary.dataSource);
        }
        if (summary.isXldLayer) {
            summary.xldManifest = validateXldManifestForLayer(summary, currentChannelMapFingerprint, currentSequencePath);
        }
        return summary;
    }

    // Marshals xLights UI access to the main thread when requests arrive on
    // the listener thread.
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
