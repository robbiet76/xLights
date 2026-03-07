/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "../xLightsMain.h"
#include "../xLightsVersion.h"

#include "nlohmann/json.hpp"

#include "../FSEQFile.h"
#include "../outputs/Controller.h"
#include "../outputs/ControllerEthernet.h"
#include "../LayoutPanel.h"
#include "../ViewsModelsPanel.h"
#include "../controllers/ControllerCaps.h"
#include "../controllers/FPP.h"
#include "../controllers/Falcon.h"
#include "../UtilFunctions.h"
#include "../ExternalHooks.h"
#include "../xLightsApp.h"
#include "../JukeboxPanel.h"
#include "../EffectsPanel.h"
#include "../outputs/E131Output.h"
#include "../../xSchedule/wxHTTPServer/wxhttpserver.h"
#include "../sequencer/MainSequencer.h"
#include "../ModelPreview.h"
#include "../ValueCurveButton.h"
#include "../effects/EffectPanelUtils.h"
#include "../utils/Curl.h"
#include <wx/uri.h>
#include <wx/debug.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filepicker.h>
#include <wx/fontpicker.h>
#include <wx/notebook.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

#include "LuaRunner.h"

#include <log4cpp/Category.hh>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <optional>
#include <set>
#include <unordered_map>

std::string xLightsFrame::FindSequence(const std::string& seq)
{
    if (FileExists(seq))
        return seq;

    if (FileExists(CurrentDir + wxFileName::GetPathSeparator() + seq))
        return CurrentDir + wxFileName::GetPathSeparator() + seq;
    
    return "";
}
static const char HTTP_ERROR_PAGE[] = "Could not process xLights Automation";
static bool HttpRequestFunction(HttpConnection &connection, HttpRequest &request) {
    return xLightsApp::__frame->ProcessHttpRequest(connection, request);
}

static wxString MIME_JSON = "application/json";
static wxString MIME_TEXT = "text/plain";

// Automation runtime guards used during scripted save/open flows.
namespace {
void AutomationAssertHandler(const wxString& file,
                             int line,
                             const wxString& func,
                             const wxString& cond,
                             const wxString& msg) {
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    logger_base.error("Suppressed wx assert in automation flow: file=%s line=%d func=%s cond=%s msg=%s",
                      (const char*)file.ToStdString().c_str(),
                      line,
                      (const char*)func.ToStdString().c_str(),
                      (const char*)cond.ToStdString().c_str(),
                      (const char*)msg.ToStdString().c_str());
}

class ScopedAutomationAssertSuppressor {
public:
    explicit ScopedAutomationAssertSuppressor(bool enabled)
        : _enabled(enabled), _previous(nullptr), _hadTrapState(false), _previousTrapState(false) {
        if (_enabled) {
            _previous = wxSetAssertHandler(AutomationAssertHandler);
#if wxDEBUG_LEVEL
            _hadTrapState = true;
            _previousTrapState = wxTrapInAssert;
            wxTrapInAssert = false;
#endif
        }
    }
    ~ScopedAutomationAssertSuppressor() {
        if (_enabled) {
#if wxDEBUG_LEVEL
            if (_hadTrapState) {
                wxTrapInAssert = _previousTrapState;
            }
#endif
            wxSetAssertHandler(_previous);
        }
    }

private:
    bool _enabled;
    wxAssertHandler_t _previous;
    bool _hadTrapState;
    bool _previousTrapState;
};
} // namespace

// Legacy dispatch adapter helpers kept in this file for readability.
namespace {
std::vector<std::string> CollectLegacyViewNames(xLightsFrame* frame);
bool RunLegacyScript(xLightsFrame* frame, const std::string& filename);
bool AddLegacyEffect(SequenceElements& sequenceElements,
                     const std::string& target,
                     const std::string& effect,
                     const std::string& settings,
                     const std::string& palette,
                     int layer,
                     int startTime,
                     int endTime);
bool GetLegacyEffectDetails(SequenceElements& sequenceElements,
                            const std::string& model,
                            int layer,
                            int id,
                            nlohmann::json& data);
bool SetLegacyEffectDetails(SequenceElements& sequenceElements,
                            MainSequencer* mainSequencer,
                            const std::string& model,
                            int layer,
                            int id,
                            const std::map<std::string, std::string>& params);
bool ImportLegacyXLightsSequence(xLightsFrame* frame,
                                 const std::string& filename,
                                 const std::string& mapname);
} // namespace

// Request query-string parsing for non-xlDo endpoints.
static std::map<std::string, std::string> ParseParams(const wxString &params) {
    std::map<std::string, std::string> p;
    std::string np = params;
    while (!np.empty()) {
        std::string np2 = np;
        size_t idx = np2.find('&');
        if (idx != std::string::npos) {
            np = np2.substr(idx + 1);
            np2 = np2.substr(0, idx);
        } else {
            np = "";
        }
        idx = np2.find('=');
        std::string value = "";
        if (idx != std::string::npos) {
            value = np2.substr(idx + 1);
            np2 = np2.substr(0, idx);
        }
        p[np2] = wxURI::Unescape(value);
    }
    return p;
}
inline bool ReadBool(const nlohmann::json& v) {
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    if (v.is_number_integer()) {
        return v.get<int>() != 0;
    }
    return v.get<std::string>() == "true" || v.get<std::string>() == "1";
}
inline bool ReadBool(const std::string &v) {
    return v == "true" || v == "1";
}

// V2 response envelope builders.
static std::string InferV2ErrorClass(const std::string& code) {
    if (code == "VALIDATION_ERROR" || code == "BAD_REQUEST" || code == "UNSUPPORTED_API_VERSION" ||
        code == "UNKNOWN_COMMAND" || code == "UNSUPPORTED_PROVIDER") {
        return "client_input";
    }
    if (code == "SEQUENCE_NOT_FOUND" || code == "SEQUENCE_NOT_OPEN" || code == "TRACK_NOT_FOUND" ||
        code == "EFFECT_NOT_FOUND" || code == "DISPLAY_ELEMENT_NOT_FOUND" || code == "JOB_NOT_FOUND") {
        return "resource_state";
    }
    if (code == "REVISION_CONFLICT" || code == "TRACK_ALREADY_EXISTS" || code == "SEQUENCE_ALREADY_OPEN" ||
        code == "UNSAVED_CHANGES" || code == "TRANSACTION_APPLY_FAILED" || code == "TRANSACTION_SEQUENCE_CHANGED") {
        return "conflict";
    }
    if (code == "REMOTE_ANALYSIS_FAILED" || code == "MEDIA_NOT_AVAILABLE" || code == "OPEN_FAILED" || code == "CREATE_FAILED") {
        return "operation_failure";
    }
    if (code == "INTERNAL_ERROR") {
        return "internal";
    }
    return "unknown";
}

static bool InferV2ErrorRetryable(const std::string& code) {
    if (code == "INTERNAL_ERROR" || code == "REMOTE_ANALYSIS_FAILED" || code == "OPEN_FAILED") {
        return true;
    }
    if (code == "TRANSACTION_APPLY_FAILED" || code == "REVISION_CONFLICT" || code == "UNSAVED_CHANGES") {
        return true;
    }
    return false;
}

static std::string BuildV2ErrorResponse(int responseCode,
                                        const std::string& cmd,
                                        const std::string& code,
                                        const std::string& message,
                                        const std::string& requestId = "",
                                        const nlohmann::json& details = nlohmann::json()) {
    nlohmann::json response;
    response["res"] = responseCode;
    response["apiVersion"] = 2;
    response["cmd"] = cmd;
    if (!requestId.empty()) {
        response["requestId"] = requestId;
    }
    response["error"] = {
        {"code", code},
        {"message", message},
        {"class", InferV2ErrorClass(code)},
        {"retryable", InferV2ErrorRetryable(code)}
    };
    if (!details.is_null() && !details.empty()) {
        response["error"]["details"] = details;
    }
    return response.dump();
}

static std::string BuildV2SuccessResponse(int responseCode,
                                          const std::string& cmd,
                                          const nlohmann::json& data,
                                          const std::string& requestId = "",
                                          const nlohmann::json& warnings = nlohmann::json::array()) {
    nlohmann::json response;
    response["res"] = responseCode;
    response["apiVersion"] = 2;
    response["cmd"] = cmd;
    if (!requestId.empty()) {
        response["requestId"] = requestId;
    }
    response["data"] = data;
    response["warnings"] = warnings;
    return response.dump();
}

static bool IsV2Command(const std::map<std::string, std::string>& params) {
    auto it = params.find("_API_VERSION");
    return it != params.end() && it->second == "2";
}

struct StagedV2Command {
    std::string cmd;
    std::map<std::string, std::string> params;
};

struct PendingV2Transaction {
    std::string id;
    std::string sequencePath;
    std::string initialRevision;
    long long createdEpochMs = 0;
    long long expiresEpochMs = 0;
    std::vector<StagedV2Command> commands;
};

static std::unordered_map<std::string, PendingV2Transaction> gPendingV2Transactions;
static constexpr long long kV2TransactionTtlMs = 10LL * 60LL * 1000LL;

struct V2JobRecord {
    std::string id;
    std::string type;
    std::string status;
    bool cancellable = false;
    int progressPct = 0;
    long long createdEpochMs = 0;
    long long startedEpochMs = 0;
    long long completedEpochMs = 0;
    long long updatedEpochMs = 0;
    nlohmann::json result = nlohmann::json::object();
    nlohmann::json error = nlohmann::json::object();
};

static std::unordered_map<std::string, V2JobRecord> gV2Jobs;
static constexpr long long kV2JobRetentionMs = 24LL * 60LL * 60LL * 1000LL;
static long long gV2JobCounter = 0;

static long long NowEpochMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string BuildSequenceRevisionToken(xLightsXmlFile* currentSeqXmlFile, const SequenceElements& sequenceElements) {
    std::string path = currentSeqXmlFile == nullptr ? "" : currentSeqXmlFile->GetFullPath().ToStdString();
    return path + "#" + std::to_string(sequenceElements.GetChangeCount());
}

static std::string BuildCurrentSequencePath(xLightsXmlFile* currentSeqXmlFile) {
    if (currentSeqXmlFile == nullptr) {
        return "";
    }
    return currentSeqXmlFile->GetFullPath().ToStdString();
}

static long long BuildSequenceLastModifiedEpochMs(xLightsXmlFile* currentSeqXmlFile) {
    if (currentSeqXmlFile == nullptr) {
        return 0;
    }
    wxFileName seqFile(wxString::FromUTF8(currentSeqXmlFile->GetFullPath().ToStdString()));
    if (!seqFile.FileExists()) {
        return 0;
    }
    wxDateTime modified = seqFile.GetModificationTime();
    if (!modified.IsValid()) {
        return 0;
    }
    return static_cast<long long>(modified.GetTicks()) * 1000LL;
}

static void PurgeExpiredTransactions() {
    long long now = NowEpochMs();
    for (auto it = gPendingV2Transactions.begin(); it != gPendingV2Transactions.end();) {
        if (it->second.expiresEpochMs > 0 && now > it->second.expiresEpochMs) {
            it = gPendingV2Transactions.erase(it);
        } else {
            ++it;
        }
    }
}

static void PurgeExpiredJobs() {
    long long now = NowEpochMs();
    for (auto it = gV2Jobs.begin(); it != gV2Jobs.end();) {
        long long anchor = it->second.completedEpochMs > 0 ? it->second.completedEpochMs : it->second.updatedEpochMs;
        if (anchor > 0 && now - anchor > kV2JobRetentionMs) {
            it = gV2Jobs.erase(it);
        } else {
            ++it;
        }
    }
}

static std::string CreateV2Job(const std::string& type, bool cancellable = false) {
    long long now = NowEpochMs();
    V2JobRecord job;
    job.id = "job-" + std::to_string(now) + "-" + std::to_string(++gV2JobCounter);
    job.type = type;
    job.status = "queued";
    job.cancellable = cancellable;
    job.progressPct = 0;
    job.createdEpochMs = now;
    job.updatedEpochMs = now;
    gV2Jobs[job.id] = job;
    return job.id;
}

static V2JobRecord* FindV2Job(const std::string& jobId) {
    auto it = gV2Jobs.find(jobId);
    if (it == gV2Jobs.end()) {
        return nullptr;
    }
    return &it->second;
}

static void MarkV2JobRunning(const std::string& jobId, int progressPct = 1) {
    V2JobRecord* job = FindV2Job(jobId);
    if (job == nullptr) {
        return;
    }
    if (progressPct < 0) {
        progressPct = 0;
    }
    if (progressPct > 99) {
        progressPct = 99;
    }
    long long now = NowEpochMs();
    job->status = "running";
    job->progressPct = progressPct;
    if (job->startedEpochMs == 0) {
        job->startedEpochMs = now;
    }
    job->updatedEpochMs = now;
}

static void MarkV2JobSucceeded(const std::string& jobId, const nlohmann::json& result) {
    V2JobRecord* job = FindV2Job(jobId);
    if (job == nullptr) {
        return;
    }
    long long now = NowEpochMs();
    job->status = "succeeded";
    job->progressPct = 100;
    job->result = result;
    job->error = nlohmann::json::object();
    if (job->startedEpochMs == 0) {
        job->startedEpochMs = now;
    }
    job->completedEpochMs = now;
    job->updatedEpochMs = now;
}

static void MarkV2JobFailed(const std::string& jobId, const std::string& code, const std::string& message) {
    V2JobRecord* job = FindV2Job(jobId);
    if (job == nullptr) {
        return;
    }
    long long now = NowEpochMs();
    job->status = "failed";
    job->progressPct = 100;
    job->result = nlohmann::json::object();
    job->error = {
        {"code", code.empty() ? "JOB_FAILED" : code},
        {"message", message.empty() ? "Job failed." : message}
    };
    if (job->startedEpochMs == 0) {
        job->startedEpochMs = now;
    }
    job->completedEpochMs = now;
    job->updatedEpochMs = now;
}

static bool MarkV2JobCancelled(const std::string& jobId, std::string& reason) {
    V2JobRecord* job = FindV2Job(jobId);
    if (job == nullptr) {
        reason = "not_found";
        return false;
    }
    if (job->status == "succeeded" || job->status == "failed" || job->status == "cancelled") {
        reason = "already_terminal";
        return false;
    }
    if (!job->cancellable) {
        reason = "not_cancellable";
        return false;
    }
    long long now = NowEpochMs();
    job->status = "cancelled";
    job->progressPct = 100;
    job->result = nlohmann::json::object();
    job->error = {
        {"code", "JOB_CANCELLED"},
        {"message", "Job cancelled."}
    };
    job->completedEpochMs = now;
    job->updatedEpochMs = now;
    reason = "cancelled";
    return true;
}

static nlohmann::json BuildV2JobData(const V2JobRecord& job) {
    nlohmann::json data;
    data["jobId"] = job.id;
    data["type"] = job.type;
    data["status"] = job.status;
    data["progressPct"] = job.progressPct;
    data["cancellable"] = job.cancellable;
    data["createdEpochMs"] = job.createdEpochMs;
    if (job.startedEpochMs > 0) {
        data["startedEpochMs"] = job.startedEpochMs;
    }
    if (job.completedEpochMs > 0) {
        data["completedEpochMs"] = job.completedEpochMs;
    }
    if (!job.result.is_null() && !job.result.empty()) {
        data["result"] = job.result;
    } else {
        data["result"] = nullptr;
    }
    if (!job.error.is_null() && !job.error.empty()) {
        data["error"] = job.error;
    } else {
        data["error"] = nullptr;
    }
    return data;
}

static bool IsV2MutatingCommand(const std::string& cmd) {
    static const std::set<std::string> mutating = {
        "sequence.save",
        "media.set",
        "timing.createTrack",
        "timing.renameTrack",
        "timing.deleteTrack",
        "timing.insertMarks",
        "timing.replaceMarks",
        "timing.deleteMarks",
        "sequencer.setDisplayElementOrder",
        "sequencer.setActiveDisplayElements",
        "effects.create",
        "effects.update",
        "effects.delete",
        "effects.deleteLayer",
        "effects.compactLayers",
        "effects.setRenderStyle",
        "effects.setPalette",
        "effects.shift",
        "effects.alignToTiming",
        "effects.clone",
        "timing.createFromAudio",
        "timing.createBarsFromBeats",
        "timing.createEnergySections"
    };
    return mutating.find(cmd) != mutating.end();
}

// Canonical list of v2 commands advertised by system.getCapabilities.
static const std::vector<std::string>& GetV2Commands() {
    // PR-2 scaffolding: extend this list as new v2 automation commands are implemented.
    static const std::vector<std::string> commands = {
        "system.getCapabilities",
        "system.getVersion",
        "system.validateCommands",
        "system.executePlan",
        "sequence.getOpen",
        "sequence.getRevision",
        "sequence.open",
        "sequence.create",
        "sequence.save",
        "sequence.close",
        "layout.getModels",
        "layout.getModel",
        "layout.getModelGeometry",
        "layout.getModelNodes",
        "layout.getCameras",
        "layout.getScene",
        "layout.getViews",
        "layout.getDisplayElements",
        "layout.getSubmodels",
        "media.get",
        "media.set",
        "media.getMetadata",
        "timing.getTracks",
        "timing.createTrack",
        "timing.renameTrack",
        "timing.deleteTrack",
        "timing.getMarks",
        "timing.insertMarks",
        "timing.replaceMarks",
        "timing.deleteMarks",
        "sequencer.getDisplayElementOrder",
        "sequencer.setDisplayElementOrder",
        "sequencer.setActiveDisplayElements",
        "effects.list",
        "effects.listDefinitions",
        "effects.getDefinition",
        "effects.getPalette",
        "effects.getRenderStyleOptions",
        "effects.create",
        "effects.update",
        "effects.delete",
        "effects.deleteLayer",
        "effects.compactLayers",
        "effects.setRenderStyle",
        "effects.setPalette",
        "effects.shift",
        "effects.alignToTiming",
        "effects.clone",
        "transactions.begin",
        "transactions.commit",
        "transactions.rollback",
        "jobs.get",
        "jobs.cancel",
        "timing.listAnalysisPlugins",
        "timing.createFromAudio",
        "timing.getTrackSummary",
        "timing.createBarsFromBeats",
        "timing.createEnergySections"
    };
    return commands;
}

static std::string ReadParamString(const std::map<std::string, std::string>& params,
                                   const std::string& key,
                                   const std::string& defaultValue = "") {
    auto it = params.find(key);
    if (it == params.end()) {
        return defaultValue;
    }
    return it->second;
}

static int ReadParamInt(const std::map<std::string, std::string>& params,
                        const std::string& key,
                        int defaultValue) {
    auto it = params.find(key);
    if (it == params.end() || it->second.empty()) {
        return defaultValue;
    }
    return wxAtoi(it->second);
}

static std::vector<std::string> ReadParamArray(const std::map<std::string, std::string>& params,
                                               const std::string& key) {
    std::vector<std::string> values;
    for (int i = 0;; i++) {
        auto it = params.find(key + "_" + std::to_string(i));
        if (it == params.end()) {
            break;
        }
        values.push_back(it->second);
    }
    return values;
}

struct TimingMarkPayload {
    int startMs = 0;
    int endMs = 0;
    std::string label;
};

static bool ParseTimingMarkArray(const std::map<std::string, std::string>& params,
                                 const std::string& key,
                                 std::vector<TimingMarkPayload>& marks,
                                 std::string& errorMessage) {
    marks.clear();
    errorMessage.clear();

    bool foundAny = false;
    for (int i = 0;; i++) {
        std::string prefix = key + "_" + std::to_string(i) + "_";
        auto itStart = params.find(prefix + "startMs");
        auto itEnd = params.find(prefix + "endMs");
        auto itLabel = params.find(prefix + "label");

        if (itStart == params.end() && itEnd == params.end() && itLabel == params.end()) {
            break;
        }

        foundAny = true;
        if (itStart == params.end()) {
            errorMessage = "Each mark requires startMs.";
            return false;
        }

        TimingMarkPayload mark;
        mark.startMs = wxAtoi(itStart->second);
        mark.endMs = (itEnd == params.end() || itEnd->second.empty() || itEnd->second == "null") ? -1 : wxAtoi(itEnd->second);
        mark.label = (itLabel == params.end() || itLabel->second == "null") ? "" : itLabel->second;

        if (mark.startMs < 0) {
            errorMessage = "mark.startMs must be >= 0.";
            return false;
        }
        if (mark.endMs != -1 && mark.endMs <= mark.startMs) {
            errorMessage = "mark.endMs must be greater than startMs.";
            return false;
        }
        marks.push_back(mark);
    }

    if (!foundAny) {
        errorMessage = "marks array is required.";
        return false;
    }
    return true;
}

static void NormalizeTimingMarks(std::vector<TimingMarkPayload>& marks, int fallbackEndMs, int frameMs) {
    int safeFrameMs = std::max(1, frameMs);
    for (size_t i = 0; i < marks.size(); i++) {
        if (marks[i].endMs != -1) {
            continue;
        }
        int derivedEnd = fallbackEndMs;
        if (i + 1 < marks.size() && marks[i + 1].startMs > marks[i].startMs) {
            derivedEnd = marks[i + 1].startMs;
        } else {
            derivedEnd = std::min(fallbackEndMs, marks[i].startMs + safeFrameMs);
        }
        marks[i].endMs = std::max(marks[i].startMs + 1, derivedEnd);
    }
}

static bool ValidateOrderedNonOverlapping(const std::vector<TimingMarkPayload>& marks, std::string& errorMessage) {
    if (marks.empty()) {
        errorMessage = "marks array must contain at least one mark.";
        return false;
    }
    for (size_t i = 1; i < marks.size(); i++) {
        if (marks[i].startMs < marks[i - 1].startMs) {
            errorMessage = "marks must be ordered by startMs.";
            return false;
        }
        if (marks[i].startMs < marks[i - 1].endMs) {
            errorMessage = "marks must not overlap.";
            return false;
        }
    }
    return true;
}

static nlohmann::json BuildDryRunWarnings(bool dryRun) {
    nlohmann::json warnings = nlohmann::json::array();
    if (dryRun) {
        warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
    }
    return warnings;
}

static std::string GetElementTypeName(const Element* element) {
    if (element == nullptr) {
        return "model";
    }
    if (element->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
        return "timing";
    }
    if (element->GetType() == ElementType::ELEMENT_TYPE_SUBMODEL) {
        return "submodel";
    }
    if (element->GetType() == ElementType::ELEMENT_TYPE_STRAND) {
        return "strand";
    }
    return "model";
}

static void RefreshEffectGridIfPresent(MainSequencer* sequencer) {
    if (sequencer != nullptr && sequencer->PanelEffectGrid != nullptr) {
        sequencer->PanelEffectGrid->Refresh();
    }
}

static nlohmann::json BuildDisplayElementOrderData(SequenceElements& sequenceElements) {
    nlohmann::json elements = nlohmann::json::array();
    size_t count = sequenceElements.GetElementCount(MASTER_VIEW);
    for (size_t i = 0; i < count; i++) {
        Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
        if (element == nullptr) {
            continue;
        }
        elements.push_back({
            {"id", element->GetName()},
            {"name", element->GetName()},
            {"type", GetElementTypeName(element)},
            {"orderIndex", static_cast<int>(i)}
        });
    }
    return elements;
}

static nlohmann::json BuildLayoutDisplayElementsData(SequenceElements& sequenceElements) {
    nlohmann::json elements = nlohmann::json::array();
    size_t count = sequenceElements.GetElementCount(MASTER_VIEW);
    for (size_t i = 0; i < count; i++) {
        Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
        if (element == nullptr) {
            continue;
        }

        nlohmann::json entry;
        std::string fullName = element->GetFullName();
        entry["id"] = fullName;
        entry["name"] = element->GetName();
        entry["type"] = GetElementTypeName(element);
        entry["orderIndex"] = static_cast<int>(i);

        size_t slash = fullName.find('/');
        if (slash != std::string::npos && slash > 0) {
            entry["parentId"] = fullName.substr(0, slash);
        }
        elements.push_back(entry);
    }
    return elements;
}

static nlohmann::json BuildLayoutSubmodelsData(const ModelManager& allModels) {
    nlohmann::json submodels = nlohmann::json::array();
    for (const auto& modelEntry : allModels) {
        Model* parent = modelEntry.second;
        if (parent == nullptr) {
            continue;
        }
        if (parent->GetDisplayAs() == "ModelGroup" || parent->GetDisplayAs() == "SubModel") {
            continue;
        }

        for (const auto& submodel : parent->GetSubModels()) {
            if (submodel == nullptr) {
                continue;
            }

            nlohmann::json groupNames = nlohmann::json::array();
            auto groups = allModels.GetGroupsContainingModel(submodel);
            for (const auto& group : groups) {
                groupNames.push_back(group);
            }

            nlohmann::json entry;
            entry["id"] = submodel->GetFullName();
            entry["name"] = submodel->GetName();
            entry["type"] = "submodel";
            entry["parentId"] = parent->GetName();
            entry["layoutGroup"] = submodel->GetLayoutGroup();
            entry["groupNames"] = groupNames;
            entry["startChannel"] = static_cast<int>(submodel->GetFirstChannel()) + 1;
            entry["endChannel"] = static_cast<int>(submodel->GetLastChannel()) + 1;
            submodels.push_back(entry);
        }
    }
    return submodels;
}

struct EffectRef {
    std::string modelName;
    int layerIndex = 0;
    EffectLayer* layer = nullptr;
    Effect* effect = nullptr;
};

static std::string MakeEffectHandle(const EffectRef& ref) {
    return ref.modelName + ":" + std::to_string(ref.layerIndex) + ":" + std::to_string(ref.effect->GetID());
}

static bool OverlapsRange(const Effect* effect, int startMs, int endMs) {
    return !(effect->GetEndTimeMS() <= startMs || effect->GetStartTimeMS() >= endMs);
}

static nlohmann::json ParseJsonObjectOrEmpty(const std::string& raw) {
    if (raw.empty() || raw == "null") {
        return nlohmann::json::object();
    }
    auto parsed = nlohmann::json::parse(raw, nullptr, false);
    if (!parsed.is_object() || parsed.is_discarded()) {
        return nlohmann::json::object();
    }
    return parsed;
}

static std::string ReadParamStringOrEnv(const std::map<std::string, std::string>& params,
                                        const std::string& key,
                                        const std::string& envKey) {
    std::string value = ReadParamString(params, key);
    if (!value.empty()) {
        return value;
    }
    wxString envValue;
    if (wxGetEnv(envKey, &envValue)) {
        return envValue.ToStdString();
    }
    return "";
}

static nlohmann::json BuildV2SequenceData(const xLightsXmlFile* sequence) {
    nlohmann::json data;
    data["name"] = sequence->GetName().ToStdString();
    data["path"] = sequence->GetFullPath().ToStdString();
    data["durationMs"] = sequence->GetSequenceDurationMS();
    data["frameMs"] = sequence->GetFrameMS();
    data["mediaFile"] = sequence->GetMediaFile().ToStdString();
    return data;
}

static nlohmann::json BuildV2ModelData(Model* model, const ModelManager& modelManager) {
    nlohmann::json data;
    data["name"] = model->GetName();
    data["type"] = model->GetDisplayAs();
    data["startChannel"] = static_cast<int>(model->GetFirstChannel()) + 1;
    data["endChannel"] = static_cast<int>(model->GetLastChannel()) + 1;
    data["layoutGroup"] = model->GetLayoutGroup();

    nlohmann::json groupNames = nlohmann::json::array();
    // ModelGroup::ContainsModel asserts when queried with a ModelGroup instance.
    // Skip reverse-membership expansion for group models to keep discovery stable.
    if (model->GetDisplayAs() != "ModelGroup") {
        auto groups = modelManager.GetGroupsContainingModel(model);
        for (const auto& group : groups) {
            groupNames.push_back(group);
        }
    }
    data["groupNames"] = groupNames;
    return data;
}

static bool ParseRemoteSections(const nlohmann::json& payload,
                                std::vector<int>& starts,
                                std::vector<int>& ends,
                                std::vector<std::string>& labels) {
    starts.clear();
    ends.clear();
    labels.clear();

    const nlohmann::json* data = &payload;
    if (payload.contains("data") && payload["data"].is_object()) {
        data = &payload["data"];
    }

    if (data->contains("sections") && (*data)["sections"].is_array()) {
        for (const auto& section : (*data)["sections"]) {
            if (!section.is_object()) {
                continue;
            }
            if (!section.contains("startMs") || !section.contains("endMs")) {
                continue;
            }
            int startMs = section["startMs"].get<int>();
            int endMs = section["endMs"].get<int>();
            if (endMs <= startMs) {
                continue;
            }
            starts.push_back(startMs);
            ends.push_back(endMs);
            if (section.contains("label") && section["label"].is_string()) {
                labels.push_back(section["label"].get<std::string>());
            } else {
                labels.push_back("");
            }
        }
        return !starts.empty();
    }

    if (data->contains("starts") && (*data)["starts"].is_array() &&
        data->contains("ends") && (*data)["ends"].is_array()) {
        const auto& startsIn = (*data)["starts"];
        const auto& endsIn = (*data)["ends"];
        size_t n = std::min(startsIn.size(), endsIn.size());
        for (size_t i = 0; i < n; i++) {
            int startMs = startsIn[i].get<int>();
            int endMs = endsIn[i].get<int>();
            if (endMs <= startMs) {
                continue;
            }
            starts.push_back(startMs);
            ends.push_back(endMs);
            labels.push_back("");
        }
        return !starts.empty();
    }

    return false;
}

// Legacy API groups: baseline sequence lifecycle, export/package, read/query, render/transfer,
// app/system control, playback/output control, layout mutation, and sequencer mutation.
#include "api/LegacySequenceCoreApi.inl"
#include "api/LegacyExportPackagingApi.inl"
#include "api/LegacyReadQueryApi.inl"
#include "api/LegacyRenderTransferApi.inl"
#include "api/LegacySystemControlApi.inl"
#include "api/LegacyPlaybackApi.inl"
#include "api/LegacyLayoutMutationApi.inl"
#include "api/LegacySequencerMutationApi.inl"

// V2 request parsing and validation infrastructure.
#include "api/V2ValidationApi.inl"
#include "api/V2RequestParsingApi.inl"

// V2 API groups: capability, sequence lifecycle, layout, media, timing, sequencer, effects, audio analysis.
#include "api/SystemV2Api.inl"
#include "api/TransactionsV2Api.inl"
#include "api/JobsV2Api.inl"
#include "api/SequenceV2Api.inl"
#include "api/LayoutV2Api.inl"
#include "api/MediaV2Api.inl"
#include "api/TimingV2Api.inl"
#include "api/SequencerV2Api.inl"
#include "api/EffectsV2Api.inl"
#include "api/TimingAnalysisV2Api.inl"

bool xLightsFrame::ProcessAutomation(std::vector<std::string> &paths,
                                     std::map<std::string, std::string> &params,
                                     const std::function<bool(const std::string &msg,
                                                              const std::string &jsonKey,
                                                              int responseCode,
                                                              bool msgIsJSON)> &sendResponse) {
    // Keep automation fully non-interactive in debug builds where wx asserts open modal dialogs.
    ScopedAutomationAssertSuppressor suppressor(true);

    if (paths.size() == 0) {
        return sendResponse("No command", "msg", 503, false);
    }

    std::string cmd = paths[0];
    // V2 dispatcher: strict command contracts and structured response envelopes.
    if (IsV2Command(params)) {
        auto requestIdIt = params.find("_REQUEST_ID");
        std::string requestId = requestIdIt == params.end() ? "" : requestIdIt->second;
        PurgeExpiredTransactions();
        PurgeExpiredJobs();
        auto requireOpenSequence = [&]() -> std::optional<bool> {
            if (CurrentSeqXmlFile != nullptr) {
                return std::nullopt;
            }
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        };

        std::string expectedRevision = ReadParamString(params, "expectedRevision");
        if (!expectedRevision.empty() && expectedRevision != "null" && IsV2MutatingCommand(cmd)) {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string currentRevision = BuildSequenceRevisionToken(CurrentSeqXmlFile, _sequenceElements);
            if (expectedRevision != currentRevision) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "REVISION_CONFLICT", "expectedRevision does not match current sequence revision.", requestId), "", 409, true);
            }
        }
        auto normalizeRangeOrError = [&](int& startMs, int& endMs, int defaultEndMs) -> std::optional<bool> {
            if (startMs < 0) {
                startMs = 0;
            }
            if (endMs < 0) {
                endMs = defaultEndMs;
            }
            if (endMs < startMs) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "endMs must be >= startMs.", requestId), "", 422, true);
            }
            return std::nullopt;
        };
        auto collectEffects = [&](const std::string& modelName,
                                  int layerIndex,
                                  int startMs,
                                  int endMs,
                                  const std::set<std::string>& effectHandlesFilter,
                                  std::vector<EffectRef>& out) {
            out.clear();

            auto collectFromElement = [&](Element* element) {
                if (element == nullptr || element->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
                    return;
                }
                int layerStart = 0;
                int layerEnd = static_cast<int>(element->GetEffectLayerCount()) - 1;
                if (layerIndex >= 0) {
                    layerStart = layerIndex;
                    layerEnd = layerIndex;
                }
                for (int li = layerStart; li <= layerEnd; li++) {
                    auto* layer = element->GetEffectLayer(li);
                    if (layer == nullptr) {
                        continue;
                    }
                    auto effects = layer->GetAllEffects();
                    for (auto* effect : effects) {
                        if (!OverlapsRange(effect, startMs, endMs)) {
                            continue;
                        }
                        EffectRef ref;
                        ref.modelName = element->GetName();
                        ref.layerIndex = li;
                        ref.layer = layer;
                        ref.effect = effect;
                        if (!effectHandlesFilter.empty() && effectHandlesFilter.find(MakeEffectHandle(ref)) == effectHandlesFilter.end()) {
                            continue;
                        }
                        out.push_back(ref);
                    }
                }
            };

            if (!modelName.empty() && modelName != "null") {
                collectFromElement(_sequenceElements.GetElement(modelName));
                return;
            }
            size_t allCount = _sequenceElements.GetElementCount(MASTER_VIEW);
            for (size_t i = 0; i < allCount; i++) {
                collectFromElement(_sequenceElements.GetElement(i, MASTER_VIEW));
            }
        };
        auto refreshEffectGrid = [&]() {
            RefreshEffectGridIfPresent(mainSequencer);
        };

        if (auto handled = automation::api::HandleTransactionsV2Command(
            this,
            _sequenceElements,
            CurrentSeqXmlFile,
            cmd,
            params,
            requestId,
            [&](std::vector<std::string>& childPaths,
                std::map<std::string, std::string>& childParams,
                const std::function<bool(const std::string&, const std::string&, int, bool)>& childSendResponse) {
                return ProcessAutomation(childPaths, childParams, childSendResponse);
            },
            sendResponse)) {
            return *handled;
        }

        if (auto handled = automation::api::HandleSystemV2Command(cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleJobsV2Command(cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleSequenceV2Command(this, _sequenceElements, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleLayoutV2Command(this, AllModels, _sequenceElements, requireOpenSequence, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleMediaV2Command(this, requireOpenSequence, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleTimingV2Command(this, _sequenceElements, requireOpenSequence, normalizeRangeOrError, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleSequencerV2Command(_sequenceElements, requireOpenSequence, refreshEffectGrid, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleEffectsV2Command(this, _sequenceElements, requireOpenSequence, normalizeRangeOrError, collectEffects, refreshEffectGrid, cmd, params, requestId, sendResponse)) {
            return *handled;
        } else if (auto handled = automation::api::HandleTimingAnalysisV2Command(this, _sequenceElements, CurrentDir.ToStdString(), requireOpenSequence, cmd, params, requestId, sendResponse)) {
            return *handled;
        }

        return sendResponse(BuildV2ErrorResponse(404, cmd, "UNKNOWN_COMMAND", "Unknown command: '" + cmd + "'.", requestId), "", 404, true);
    }

    // Legacy dispatcher: grouped by historical xlDo API domain.
    // Group 1: Core sequence lifecycle and save/open compatibility.
    if (auto handled = automation::api::HandleLegacySequenceCoreCommand(this, _sequenceElements, _promptBatchRenderIssues, _renderMode, xlightsFilename, cmd, paths, params, sendResponse)) {
        return *handled;
    // Group 2: Export, packaging, and video preview operations.
    } else if (auto handled = automation::api::HandleLegacyExportPackagingCommand(
                   AllModels,
                   _outputModelManager,
                   _lowDefinitionRender,
                   CurrentDir,
                   CurrentSeqXmlFile,
                   [&](const wxString& filename) { ExportModels(filename); },
                   [&](const std::string& model, const std::string& filename, const std::string& format, bool doRender) {
                       return DoExportModel(0, 0, model, filename, format, doRender);
                   },
                   [&]() { return PackageSequence(false); },
                   [&]() { return PackageDebugFiles(false); },
                   [&](const wxString& filename) { return ExportVideoPreview(filename); },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 3: Read/query endpoints for models, views, controllers, and effect IDs.
    } else if (auto handled = automation::api::HandleLegacyReadQueryCommand(
                   AllModels,
                   _outputManager,
                   _sequenceElements,
                   CurrentSeqXmlFile,
                   [&]() { return CollectLegacyViewNames(this); },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 4: Render and transfer workflows (batch render, controller/FPP upload, sequence checks).
    } else if (auto handled = automation::api::HandleLegacyRenderTransferCommand(
                   this,
                   _outputManager,
                   _outputModelManager,
                   AllModels,
                   _lowDefinitionRender,
                   _promptBatchRenderIssues,
                   _renderMode,
                   _saveLowDefinitionRender,
                   mRendering,
                   CurrentDir,
                   CurrentSeqXmlFile,
                   [&](const std::string& seq) { return FindSequence(seq); },
                   [&]() { RenderAll(); },
                   [&](const wxArrayString& files, bool promptIssues) { OpenRenderAndSaveSequences(files, promptIssues); },
                   [&]() { wxYield(); },
                   [&]() { RecalcModels(); },
                   [&](const std::string& controllerName) { return GetControllerCaps(controllerName); },
                   [&](Controller* c, wxString& message) { return UploadInputToController(c, message); },
                   [&](Controller* c, wxString& message) { return UploadOutputToController(c, message); },
                   [&](FPP* fpp) {
                       int pw, ph;
                       GetLayoutPreview()->GetVirtualCanvasSize(pw, ph);
                       std::map<std::string, std::string> virtualDisplayData;
                       FPP::CreateVirtualDisplayMap(AllModels, AllObjects, pw, ph, virtualDisplayData);
                       fpp->UploadDisplayMap(virtualDisplayData);
                       fpp->SetRestartFlag(true);
                   },
                   [&](const std::string& xsq) { return xLightsXmlFile::GetFSEQForXSQ(xsq, GetFseqDirectory()); },
                   [&](const std::string& xsq) { return xLightsXmlFile::GetMediaForXSQ(xsq, CurrentDir, GetMediaFolders()); },
                   [&](const std::string& seq) { return OpenAndCheckSequence(seq); },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 5: Application/system control (show folder, save layout, close app, controller browser links).
    } else if (auto handled = automation::api::HandleLegacySystemControlCommand(
                   _sequenceElements,
                   CurrentSeqXmlFile,
                   mSavedChangeCount,
                   UnsavedRgbEffectsChanges,
                   UnsavedNetworkChanges,
                   showDirectory,
                   _outputManager,
                   [&]() { return layoutPanel->SaveEffects(); },
                   [&]() { return SaveNetworksFile(); },
                   [&]() {
                       displayElementsPanel->SetSequenceElementsModelsViews(nullptr, nullptr, nullptr, nullptr, nullptr);
                       layoutPanel->ClearUndo();
                   },
                   [&](const std::string& folder) { SetDir(folder, true); },
                   [&]() {
                       wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, wxID_EXIT);
                       wxPostEvent(this, evt);
                   },
                   [&](const std::string& url) { ::wxLaunchDefaultBrowser(url); },
                   [&]() { return GetFseqDirectory(); },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 6: Playback and output control (lights, jukebox, e131 tag).
    } else if (auto handled = automation::api::HandleLegacyPlaybackCommand(
                   CurrentSeqXmlFile,
                   [&]() { EnableOutputs(true); },
                   [&]() { DisableOutputs(); },
                   [&](int button) { jukeboxPanel->PlayItem(button); },
                   [&]() { return jukeboxPanel->GetTooltipsJSON(); },
                   [&]() { return jukeboxPanel->GetEffectPresentJSON(); },
                   [&]() { return E131Output::GetTag(); },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 7: Layout/controller mutation helpers.
    } else if (auto handled = automation::api::HandleLegacyLayoutMutationCommand(
                   AllModels,
                   _outputManager,
                   _outputModelManager,
                   CurrentSeqXmlFile,
                   [&]() { MarkEffectsFileDirty(); },
                   [&](const std::string& view) {
                       displayElementsPanel->SelectView(view);
                       displayElementsPanel->DoMakeMaster();
                   },
                   [&](const std::string& model, const std::string& propKey, const std::string& propData) {
                       layoutPanel->SelectModel(model);
                       wxPropertyGridEvent event2;
                       event2.SetPropertyGrid(layoutPanel->GetPropertyEditor());
                       wxStringProperty wsp("Model", propKey, propData);
                       event2.SetProperty(&wsp);
                       wxVariant value(propData);
                       event2.SetPropertyValue(value);
                       layoutPanel->OnPropertyGridChange(event2);
                       _outputModelManager.AddASAPWork(OutputModelManager::WORK_RGBEFFECTS_CHANGE, "Automation:setModelProperty");
                       _outputModelManager.AddASAPWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation:setModelProperty");
                       _outputModelManager.AddASAPWork(OutputModelManager::WORK_RELOAD_PROPERTYGRID, "Automation:setModelProperty");
                       return true;
                   },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    // Group 8: Sequencer/effect mutation and import helpers.
    } else if (auto handled = automation::api::HandleLegacySequencerMutationCommand(
                   CurrentSeqXmlFile,
                   [&](const std::string& filename) { return RunLegacyScript(this, filename); },
                   [&](const std::string& target, const std::string& source, bool erase) {
                       return CloneXLightsEffects(target, source, _sequenceElements, erase);
                   },
                   [&](const std::string& target, const std::string& effect, const std::string& settings, const std::string& palette, int layer, int startTime, int endTime) {
                       return AddLegacyEffect(_sequenceElements, target, effect, settings, palette, layer, startTime, endTime);
                   },
                   [&]() { return CleanupRGBEffectsFileLocations(); },
                   [&]() { return CleanupSequenceFileLocations(); },
                   [&](const std::string& model, int layer, int id, nlohmann::json& data) {
                       return GetLegacyEffectDetails(_sequenceElements, model, layer, id, data);
                   },
                   [&](const std::string& model, int layer, int id, const std::map<std::string, std::string>& p) {
                       return SetLegacyEffectDetails(_sequenceElements, mainSequencer, model, layer, id, p);
                   },
                   [&](const std::string& filename, const std::string& mapname) {
                       return ImportLegacyXLightsSequence(this, filename, mapname);
                   },
                   [&]() { mainSequencer->PanelEffectGrid->Refresh(); },
                    cmd,
                    params,
                    sendResponse)) {
        return *handled;
    }
    return false;
}


bool xLightsFrame::ProcessHttpRequest(HttpConnection& connection, HttpRequest& request)
{
    wxString uri = request.URI();
    wxString params = "";

    if (uri.find('?') != std::string::npos) {
        params = uri.substr(uri.find('?') + 1);
        uri = uri.substr(0, uri.find('?'));
    }
    std::vector<std::string> paths;
    std::map<std::string, std::string> paramMap = ParseParams(params);

    if (uri[0] == '/') {
        uri = uri.substr(1);
    }
    while (uri.find('/') != std::string::npos) {
        wxString p = uri.substr(0, uri.find('/'));
        paths.push_back(wxURI::Unescape(p));
        uri = uri.substr(uri.find('/') + 1);
    }
    paths.push_back(wxURI::Unescape(uri));

    wxString accept = request["Accept"];
    if (paths[0] == "xlDoAutomation") {
        paths.clear();
        paramMap.clear();
        accept = MIME_JSON;

        std::string errorBody;
        int errorStatus = 500;
        if (!ParseXlDoAutomationBody(request.Data().ToStdString(), paths, paramMap, errorBody, errorStatus)) {
            HttpResponse resp(connection, request, (HttpStatus::HttpStatusCode)errorStatus);
            resp.AddHeader("access-control-allow-origin", "*");
            resp.MakeFromText(errorBody, MIME_JSON);
            connection.SendResponse(resp);
            return true;
        }
    } else {
        paramMap["_METHOD"] = request.Method();
        if (request.Method() == "POST" || request.Method() == "PUT") {
            paramMap["_DATA"] = request.Data();
        }
    }

    return ProcessAutomation(paths, paramMap, [&](const std::string& msg, const std::string& jsonKey, int responseCode, bool isJson) {
        static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
        HttpResponse resp(connection, request, (HttpStatus::HttpStatusCode)responseCode);
        resp.AddHeader("access-control-allow-origin", "*");

        if (accept == MIME_JSON) {
            if (isJson) {
                if (jsonKey == "") {
                    resp.MakeFromText(msg, MIME_JSON);
                } else {
                    wxString json = "{\"" + jsonKey + "\":" + msg + "}";
                    resp.MakeFromText(json, MIME_JSON);
                }
            } else {
                wxString json = "{\"" + jsonKey + "\":\"" + msg + "\"}";
                resp.MakeFromText(json, MIME_JSON);
            }
        } else if (isJson) {
            resp.MakeFromText(msg, MIME_JSON);
        } else {
            resp.MakeFromText(msg, MIME_TEXT);
        }
        // The problem here is the connection may no longer be valid ... but i am not sure how to safely detect this
        // This means if the client suddenly disconnects then xLights crashes on the SendResponse call as connection and request objects have all been destroyed from under us with no way to know it has happened
        // adding this check helps but it still has a race condition
        if (_automationServer->IsConnectionValid(&connection)) {
            connection.SendResponse(resp);
            return true;
        } else {
            logger_base.warn("Automation did not send result because connection lost.");
        }
        return false;
    });
}

void xLightsFrame::StartAutomationListener()
{
    static log4cpp::Category &logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    if (_automationServer != nullptr) {
        _automationServer->Stop();
        delete _automationServer;
        _automationServer = nullptr;
    }

    if (_xFadePort == 0) return;
    
    
    HttpServer *server = new HttpServer();
    HttpContext ctx;
    ctx.Port = ::GetxFadePort(_xFadePort);

    ctx.RequestHandler = HttpRequestFunction;
    ctx.MessageHandler = nullptr;

    // default error pages content
    ctx.ErrorPage400 = HTTP_ERROR_PAGE;
    ctx.ErrorPage404 = HTTP_ERROR_PAGE;

    if (!server->Start(ctx)) {
        logger_base.debug("xLights Automation could not listen on %d", ::GetxFadePort(_xFadePort));
        delete server;
        return;
    }
    logger_base.debug("xLights Automation listening on %d", ::GetxFadePort(_xFadePort));
    _automationServer = server;
}

std::string xLightsFrame::ProcessxlDoAutomation(const std::string& msg)
{
    std::vector<std::string> paths;
    std::map<std::string, std::string> paramMap;

    std::string errorBody;
    int errorStatus = 500;
    if (!ParseXlDoAutomationBody(msg, paths, paramMap, errorBody, errorStatus)) {
        return errorBody;
    }

    std::string result;
    bool processed = ProcessAutomation(paths, paramMap, [&](const std::string &msg,
                                                            const std::string &jsonKey,
                                                            int responseCode,
                                                            bool isJson) {
        if (isJson) {
            if (jsonKey == "") {
                if (!msg.empty() && msg[0] == '{' && msg.find("\"res\"") != std::string::npos) {
                    result = msg;
                } else {
                    result = "{\"res\":" + std::to_string(responseCode) +"," + msg.substr(1);
                }
            } else {
                result = "{\"res\":" + std::to_string(responseCode) +",\"" + jsonKey + "\":" + msg + "}";
            }
        } else {
            result = "{\"res\":" + std::to_string(responseCode) +",\"" + jsonKey + "\":\"" + msg + "\"}";
        }
        return true;
    });

    if (!processed) {
        return wxString::Format("{\"res\":504,\"msg\":\"Unknown command: '%s'.\"}", paths[0]);
    }
    return result;
}

// ---- Bottom helper implementations (legacy dispatcher adapters) ----
namespace {
std::vector<std::string> CollectLegacyViewNames(xLightsFrame* frame) {
    std::vector<std::string> names;
    auto allViews = frame->GetViewsManager()->GetViews();
    names.reserve(allViews.size());
    for (auto* view : allViews) {
        names.emplace_back(view->GetName());
    }
    return names;
}

bool RunLegacyScript(xLightsFrame* frame, const std::string& filename) {
    LuaRunner runner(frame);
    return runner.Run_Script(filename, [](std::string const&) {});
}

bool AddLegacyEffect(SequenceElements& sequenceElements,
                     const std::string& target,
                     const std::string& effect,
                     const std::string& settings,
                     const std::string& palette,
                     int layer,
                     int startTime,
                     int endTime) {
    Element* to = sequenceElements.GetElement(target);
    if (to == nullptr) {
        return false;
    }
    sequenceElements.get_undo_mgr().CreateUndoStep();
    while (to->GetEffectLayerCount() < layer + 1) {
        to->AddEffectLayer();
    }
    auto valid = to->GetEffectLayer(layer)->AddEffect(0, effect, settings, palette, startTime, endTime, 0, false);
    return valid != nullptr;
}

bool GetLegacyEffectDetails(SequenceElements& sequenceElements,
                            const std::string& model,
                            int layer,
                            int id,
                            nlohmann::json& data) {
    Element* ele = sequenceElements.GetElement(model);
    if (ele == nullptr) {
        return false;
    }
    auto* lay = ele->GetEffectLayer(layer);
    if (lay == nullptr) {
        return false;
    }
    auto* eff = lay->GetEffectFromID(id);
    if (eff == nullptr) {
        return false;
    }
    data["name"] = eff->GetEffectName();
    auto settings = nlohmann::json::parse(eff->GetSettingsAsJSON(), nullptr, false);
    data["settings"] = settings.is_discarded() ? nlohmann::json::object() : settings;
    auto palette = nlohmann::json::parse(eff->GetPaletteAsJSON(), nullptr, false);
    data["palette"] = palette.is_discarded() ? nlohmann::json::object() : palette;
    data["startTime"] = eff->GetStartTimeMS();
    data["endTime"] = eff->GetEndTimeMS();
    data["selected"] = eff->GetSelected();
    return true;
}

bool SetLegacyEffectDetails(SequenceElements& sequenceElements,
                            MainSequencer* mainSequencer,
                            const std::string& model,
                            int layer,
                            int id,
                            const std::map<std::string, std::string>& params) {
    Element* ele = sequenceElements.GetElement(model);
    if (ele == nullptr) {
        return false;
    }
    auto* lay = ele->GetEffectLayer(layer);
    if (lay == nullptr) {
        return false;
    }
    auto* eff = lay->GetEffectFromID(id);
    if (eff == nullptr) {
        return false;
    }
    auto it = params.find("name");
    if (it != params.end() && !it->second.empty()) {
        eff->SetEffectName(it->second);
    }
    it = params.find("startTime");
    if (it != params.end() && !it->second.empty()) {
        eff->SetStartTimeMS(std::stoi(it->second));
    }
    it = params.find("endTime");
    if (it != params.end() && !it->second.empty()) {
        eff->SetEndTimeMS(std::stoi(it->second));
    }
    it = params.find("settings");
    if (it != params.end() && !it->second.empty()) {
        eff->SetSettings(it->second, true, true);
    }
    it = params.find("palette");
    if (it != params.end() && !it->second.empty()) {
        eff->SetColourOnlyPalette(it->second, true);
    }
    if (mainSequencer != nullptr) {
        mainSequencer->SelectEffect(eff);
    }
    return true;
}

bool ImportLegacyXLightsSequence(xLightsFrame* frame,
                                 const std::string& filename,
                                 const std::string& mapname) {
    frame->ImportXLights(wxFileName(filename), mapname);
    wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
    wxPostEvent(frame, eventRowHeaderChanged);
    return true;
}
} // namespace

/*
 TODO backlog (legacy xlDo command names that remain unimplemented in this file):
 - runDiscovery
 - exportModelAsCustom
 - shiftAllEffects
 - shiftSelectedEffects
 - unselectEffects
 - selectEffectsOnModel
 - selectAllEffectsOnAllModels
 - turnOnOutputToLights
 - turnOffOutputToLights
 - playSequence
 - printLayout
 - printWiring
 - exportLayoutImage
 - exportWiringImage
 - hinksPixExport
 - purgeDownloadCache
 - purgeRenderCache
 - convertSequence
 - prepareAudio
 - resetToDefaults
 - resetWindowLayout
 - setAudioVolume
 - setAudioSpeed
 - gotoZoom
 - importSequence
*/
