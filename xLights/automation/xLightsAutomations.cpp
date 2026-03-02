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
#include "../outputs/E131Output.h"
#include "../../xSchedule/wxHTTPServer/wxhttpserver.h"
#include "../sequencer/MainSequencer.h"
#include "../ModelPreview.h"
#include "../utils/Curl.h"
#include <wx/uri.h>
#include <wx/debug.h>

#include "LuaRunner.h"

#include <log4cpp/Category.hh>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <set>

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
        : _enabled(enabled), _previous(nullptr) {
        if (_enabled) {
            _previous = wxSetAssertHandler(AutomationAssertHandler);
        }
    }
    ~ScopedAutomationAssertSuppressor() {
        if (_enabled) {
            wxSetAssertHandler(_previous);
        }
    }

private:
    bool _enabled;
    wxAssertHandler_t _previous;
};
} // namespace

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

static std::string BuildV2ErrorResponse(int responseCode,
                                        const std::string& cmd,
                                        const std::string& code,
                                        const std::string& message,
                                        const std::string& requestId = "") {
    nlohmann::json response;
    response["res"] = responseCode;
    response["apiVersion"] = 2;
    response["cmd"] = cmd;
    if (!requestId.empty()) {
        response["requestId"] = requestId;
    }
    response["error"] = { {"code", code}, {"message", message} };
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

static const std::vector<std::string>& GetV2Commands() {
    // PR-2 scaffolding: extend this list as new v2 automation commands are implemented.
    static const std::vector<std::string> commands = {
        "system.getCapabilities",
        "system.validateCommands",
        "sequence.getOpen",
        "sequence.open",
        "sequence.create",
        "sequence.save",
        "sequence.close",
        "layout.getModels",
        "layout.getModel",
        "layout.getViews",
        "layout.getDisplayElements",
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
        "effects.list",
        "effects.create",
        "effects.update",
        "effects.delete",
        "effects.shift",
        "effects.alignToTiming",
        "effects.clone",
        "timing.listAnalysisPlugins",
        "timing.createFromAudio",
        "timing.getTrackSummary",
        "timing.createBarsFromBeats",
        "timing.createEnergySections"
    };
    return commands;
}

static bool ReadScalarParam(const nlohmann::json& value, std::string& out) {
    if (value.is_string()) {
        out = value.get<std::string>();
        return true;
    }
    if (value.is_boolean()) {
        out = value.get<bool>() ? "true" : "false";
        return true;
    }
    if (value.is_number_unsigned()) {
        out = std::to_string(value.get<uint64_t>());
        return true;
    }
    if (value.is_number_integer()) {
        out = std::to_string(value.get<int64_t>());
        return true;
    }
    if (value.is_number_float()) {
        out = std::to_string(value.get<double>());
        return true;
    }
    return false;
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

static bool HasArrayParam(const nlohmann::json& params, const std::string& key) {
    return params.contains(key) && params[key].is_array();
}

static std::string GetValidationErrorMessage(const std::string& message) {
    return message.empty() ? "Validation failed." : message;
}

static bool ValidateTimingMarksParamShape(const nlohmann::json& params, std::string& errorMessage) {
    if (!HasArrayParam(params, "marks") || params["marks"].empty()) {
        errorMessage = "timing.insertMarks requires non-empty params.marks.";
        return false;
    }

    int previousStart = -1;
    int previousEnd = -1;
    bool hasPreviousEnd = false;
    for (size_t i = 0; i < params["marks"].size(); i++) {
        const auto& mark = params["marks"][i];
        if (!mark.is_object()) {
            errorMessage = "marks entries must be objects.";
            return false;
        }
        if (!mark.contains("startMs") || !mark["startMs"].is_number_integer()) {
            errorMessage = "marks[].startMs must be an integer.";
            return false;
        }
        int startMs = mark["startMs"].get<int>();
        if (startMs < 0) {
            errorMessage = "marks[].startMs must be >= 0.";
            return false;
        }

        bool hasEnd = mark.contains("endMs") && !mark["endMs"].is_null();
        int endMs = -1;
        if (hasEnd) {
            if (!mark["endMs"].is_number_integer()) {
                errorMessage = "marks[].endMs must be an integer when provided.";
                return false;
            }
            endMs = mark["endMs"].get<int>();
            if (endMs <= startMs) {
                errorMessage = "marks[].endMs must be > startMs.";
                return false;
            }
        }
        if (mark.contains("label") && !(mark["label"].is_string() || mark["label"].is_null())) {
            errorMessage = "marks[].label must be a string when provided.";
            return false;
        }

        if (previousStart != -1 && startMs < previousStart) {
            errorMessage = "marks must be ordered by startMs.";
            return false;
        }
        if (hasPreviousEnd && startMs < previousEnd) {
            errorMessage = "marks must not overlap.";
            return false;
        }
        previousStart = startMs;
        if (hasEnd) {
            previousEnd = endMs;
            hasPreviousEnd = true;
        } else {
            hasPreviousEnd = false;
        }
    }

    return true;
}

static bool ValidateDisplayOrderParams(const nlohmann::json& params, std::string& errorMessage) {
    if (!HasArrayParam(params, "orderedIds") || params["orderedIds"].empty()) {
        errorMessage = "sequencer.setDisplayElementOrder requires non-empty params.orderedIds.";
        return false;
    }

    std::set<std::string> seen;
    for (const auto& id : params["orderedIds"]) {
        if (!id.is_string() || id.get<std::string>().empty()) {
            errorMessage = "orderedIds must contain non-empty strings.";
            return false;
        }
        if (!seen.insert(id.get<std::string>()).second) {
            errorMessage = "orderedIds must not contain duplicates.";
            return false;
        }
    }
    return true;
}

static bool IsValidEffectSelectorValue(const nlohmann::json& value) {
    if (value.is_string()) {
        std::string s = value.get<std::string>();
        return !s.empty() && s != "null";
    }
    if (value.is_number_integer()) {
        return value.get<int>() > 0;
    }
    return false;
}

static bool ValidateEffectSelectorParams(const nlohmann::json& params, std::string& errorMessage) {
    bool hasSelector = false;

    if (params.contains("modelName")) {
        if (!params["modelName"].is_string() || params["modelName"].get<std::string>().empty()) {
            errorMessage = "modelName must be a non-empty string when provided.";
            return false;
        }
        hasSelector = true;
    }
    if (params.contains("layerIndex")) {
        if (!params["layerIndex"].is_number_integer() || params["layerIndex"].get<int>() < 0) {
            errorMessage = "layerIndex must be >= 0 when provided.";
            return false;
        }
    }
    if (params.contains("effectId")) {
        if (!IsValidEffectSelectorValue(params["effectId"])) {
            errorMessage = "effectId must be a non-empty string or positive integer.";
            return false;
        }
        hasSelector = true;
    }
    if (params.contains("effectIds")) {
        if (!params["effectIds"].is_array() || params["effectIds"].empty()) {
            errorMessage = "effectIds must be a non-empty array when provided.";
            return false;
        }
        for (const auto& id : params["effectIds"]) {
            if (!IsValidEffectSelectorValue(id)) {
                errorMessage = "effectIds entries must be non-empty strings or positive integers.";
                return false;
            }
        }
        hasSelector = true;
    }

    if (!hasSelector) {
        errorMessage = "An effect selector is required (modelName, effectId, or effectIds).";
        return false;
    }
    return true;
}

static bool ValidateBatchCommandShape(const nlohmann::json& command,
                                      const std::set<std::string>& commandSet,
                                      std::string& errorCode,
                                      std::string& errorMessage) {
    if (!command.is_object()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[] entries must be objects.";
        return false;
    }
    if (!command.contains("cmd") || !command["cmd"].is_string() || command["cmd"].get<std::string>().empty()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[].cmd must be a non-empty string.";
        return false;
    }

    std::string childCmd = command["cmd"].get<std::string>();
    if (commandSet.find(childCmd) == commandSet.end()) {
        errorCode = "UNKNOWN_COMMAND";
        errorMessage = "Unsupported command: '" + childCmd + "'.";
        return false;
    }

    const nlohmann::json params = command.contains("params") ? command["params"] : nlohmann::json::object();
    if (!params.is_object()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[].params must be an object.";
        return false;
    }

    if (command.contains("options")) {
        const auto& options = command["options"];
        if (!options.is_object()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options must be an object.";
            return false;
        }
        if (options.contains("requestId") && !options["requestId"].is_string()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options.requestId must be a string.";
            return false;
        }
        if (options.contains("dryRun") && !options["dryRun"].is_boolean() && !options["dryRun"].is_number_integer()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options.dryRun must be a boolean or integer.";
            return false;
        }
    }

    if (childCmd == "system.validateCommands") {
        errorCode = "VALIDATION_ERROR";
        errorMessage = "Nested system.validateCommands is not allowed.";
        return false;
    }

    if (childCmd == "sequence.open") {
        if (!params.contains("file") || !params["file"].is_string() || params["file"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "sequence.open requires params.file.";
            return false;
        }
    } else if (childCmd == "sequence.create") {
        if (!params.contains("frameMs") || !params["frameMs"].is_number_integer() || params["frameMs"].get<int>() <= 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "sequence.create requires params.frameMs > 0.";
            return false;
        }
        bool hasMediaFile = params.contains("mediaFile") && !params["mediaFile"].is_null() &&
                            params["mediaFile"].is_string() && !params["mediaFile"].get<std::string>().empty();
        if (!hasMediaFile) {
            if (!params.contains("durationMs") || !params["durationMs"].is_number_integer() || params["durationMs"].get<int>() <= 0) {
                errorCode = "VALIDATION_ERROR";
                errorMessage = "sequence.create requires params.durationMs > 0 when mediaFile is absent.";
                return false;
            }
        }
    } else if (childCmd == "layout.getModel") {
        if (!params.contains("name") || !params["name"].is_string() || params["name"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "layout.getModel requires params.name.";
            return false;
        }
    } else if (childCmd == "media.set") {
        if (!params.contains("mediaFile") || !params["mediaFile"].is_string() || params["mediaFile"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "media.set requires params.mediaFile.";
            return false;
        }
    } else if (childCmd == "timing.createTrack") {
        if (!params.contains("trackName") || !params["trackName"].is_string() || params["trackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "timing.createTrack requires params.trackName.";
            return false;
        }
    } else if (childCmd == "timing.renameTrack") {
        if (!params.contains("trackName") || !params["trackName"].is_string() ||
            !params.contains("newTrackName") || !params["newTrackName"].is_string() ||
            params["trackName"].get<std::string>().empty() || params["newTrackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "timing.renameTrack requires params.trackName and params.newTrackName.";
            return false;
        }
    } else if (childCmd == "timing.deleteTrack" || childCmd == "timing.getMarks" ||
               childCmd == "timing.insertMarks" || childCmd == "timing.replaceMarks" ||
               childCmd == "timing.deleteMarks" || childCmd == "timing.getTrackSummary") {
        if (!params.contains("trackName") || !params["trackName"].is_string() || params["trackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = childCmd + " requires params.trackName.";
            return false;
        }
        if (childCmd == "timing.insertMarks" || childCmd == "timing.replaceMarks") {
            if (!ValidateTimingMarksParamShape(params, errorMessage)) {
                errorCode = "VALIDATION_ERROR";
                return false;
            }
        }
    } else if (childCmd == "sequencer.setDisplayElementOrder") {
        if (!ValidateDisplayOrderParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
    } else if (childCmd == "effects.create") {
        if (!params.contains("modelName") || !params["modelName"].is_string() || params["modelName"].get<std::string>().empty() ||
            !params.contains("layerIndex") || !params["layerIndex"].is_number_integer() || params["layerIndex"].get<int>() < 0 ||
            !params.contains("effectName") || !params["effectName"].is_string() || params["effectName"].get<std::string>().empty() ||
            !params.contains("startMs") || !params["startMs"].is_number_integer() ||
            !params.contains("endMs") || !params["endMs"].is_number_integer() ||
            params["endMs"].get<int>() <= params["startMs"].get<int>()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.create requires modelName, layerIndex>=0, effectName, and endMs>startMs.";
            return false;
        }
    } else if (childCmd == "effects.alignToTiming") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
        if (!params.contains("timingTrackName") || !params["timingTrackName"].is_string() || params["timingTrackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.alignToTiming requires timingTrackName.";
            return false;
        }
    } else if (childCmd == "effects.shift") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
        if (!params.contains("deltaMs") || !params["deltaMs"].is_number_integer() || params["deltaMs"].get<int>() == 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.shift requires deltaMs != 0.";
            return false;
        }
    } else if (childCmd == "effects.update" || childCmd == "effects.delete") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
    } else if (childCmd == "effects.clone") {
        if (!params.contains("sourceModelName") || !params["sourceModelName"].is_string() || params["sourceModelName"].get<std::string>().empty() ||
            !params.contains("sourceLayerIndex") || !params["sourceLayerIndex"].is_number_integer() || params["sourceLayerIndex"].get<int>() < 0 ||
            !HasArrayParam(params, "targetModels") || params["targetModels"].empty() ||
            !params.contains("targetLayerIndex") || !params["targetLayerIndex"].is_number_integer() || params["targetLayerIndex"].get<int>() < 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.clone requires sourceModelName/sourceLayerIndex/targetModels/targetLayerIndex.";
            return false;
        }
    }

    return true;
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

static bool ParseXlDoAutomationBody(const std::string& body,
                                    std::vector<std::string>& paths,
                                    std::map<std::string, std::string>& paramMap,
                                    std::string& errorBody,
                                    int& errorStatus) {
    nlohmann::json val;
    try {
        val = nlohmann::json::parse(body);
    } catch (const std::exception&) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Malformed JSON request body.");
        return false;
    }

    if (!val.is_object()) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Request body must be a JSON object.");
        return false;
    }

    try {
        if (val.contains("apiVersion")) {
            if (!val["apiVersion"].is_number_integer()) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "apiVersion must be an integer.");
                return false;
            }
            int apiVersion = val["apiVersion"].get<int>();
            if (apiVersion != 2) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "UNSUPPORTED_API_VERSION", "Only apiVersion=2 is supported.");
                return false;
            }

            if (!val.contains("cmd") || !val["cmd"].is_string() || val["cmd"].get<std::string>().empty()) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Missing cmd.");
                return false;
            }

            paths.push_back(val["cmd"].get<std::string>());
            paramMap["_API_VERSION"] = "2";

            if (val.contains("options")) {
                if (!val["options"].is_object()) {
                    errorStatus = 400;
                    errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options must be an object.");
                    return false;
                }

                auto options = val["options"];
                if (options.contains("requestId")) {
                    if (!options["requestId"].is_string()) {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options.requestId must be a string.");
                        return false;
                    }
                    paramMap["_REQUEST_ID"] = options["requestId"].get<std::string>();
                }
                if (options.contains("dryRun")) {
                    if (options["dryRun"].is_boolean()) {
                        paramMap["_DRY_RUN"] = options["dryRun"].get<bool>() ? "true" : "false";
                    } else if (options["dryRun"].is_number_integer()) {
                        paramMap["_DRY_RUN"] = options["dryRun"].get<int>() != 0 ? "true" : "false";
                    } else {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options.dryRun must be a boolean or integer.");
                        return false;
                    }
                }
            }

            if (val.contains("params")) {
                if (!val["params"].is_object()) {
                    errorStatus = 400;
                    errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params must be an object.");
                    return false;
                }

                for (auto [name, value] : val["params"].items()) {
                    if (name == "commands" && value.is_array()) {
                        paramMap[name] = value.dump();
                        continue;
                    }

                    if (value.is_array()) {
                        for (size_t i = 0; i < value.size(); i++) {
                            if (value[i].is_object()) {
                                for (auto [childName, childValue] : value[i].items()) {
                                    std::string scalar;
                                    if (!ReadScalarParam(childValue, scalar)) {
                                        errorStatus = 400;
                                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params object-array values must be string, number, or boolean.");
                                        return false;
                                    }
                                    paramMap[name + "_" + std::to_string(i) + "_" + childName] = scalar;
                                }
                                continue;
                            }
                            std::string scalar;
                            if (!ReadScalarParam(value[i], scalar)) {
                                errorStatus = 400;
                                errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params values must be string, number, or boolean.");
                                return false;
                            }
                            paramMap[name + "_" + std::to_string(i)] = scalar;
                        }
                        continue;
                    }

                    if (value.is_object()) {
                        paramMap[name] = value.dump();
                        continue;
                    }

                    std::string scalar;
                    if (!ReadScalarParam(value, scalar)) {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params values must be string, number, or boolean.");
                        return false;
                    }
                    paramMap[name] = scalar;
                }
            }

            paramMap["_METHOD"] = "POST";
            return true;
        }

        if (!val.contains("cmd")) {
            errorStatus = 503;
            errorBody = "{\"res\":503,\"msg\":\"Missing cmd.\"}";
            return false;
        }
        if (!val["cmd"].is_string() || val["cmd"].get<std::string>().empty()) {
            errorStatus = 400;
            errorBody = "{\"res\":400,\"msg\":\"cmd must be a non-empty string.\"}";
            return false;
        }

        paths.push_back(val["cmd"].get<std::string>());
        for (auto [name, value] : val.items()) {
            if (name == "cmd") {
                continue;
            }

            if (value.is_array()) {
                for (size_t x = 0; x < value.size(); x++) {
                    std::string scalar;
                    if (!ReadScalarParam(value[x], scalar)) {
                        errorStatus = 400;
                        errorBody = "{\"res\":400,\"msg\":\"Array params must contain string, number, or boolean values.\"}";
                        return false;
                    }
                    paramMap[name + "_" + std::to_string(x)] = scalar;
                }
            } else {
                std::string scalar;
                if (!ReadScalarParam(value, scalar)) {
                    errorStatus = 400;
                    errorBody = "{\"res\":400,\"msg\":\"Params must be string, number, boolean, or arrays of those values.\"}";
                    return false;
                }
                paramMap[name] = scalar;
            }
        }
    } catch (const std::exception&) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Request contains values that are out of supported range.");
        return false;
    }

    paramMap["_METHOD"] = paramMap.empty() ? "GET" : "POST";
    return true;
}

#include "api/SystemV2Api.inl"
#include "api/SequenceV2Api.inl"
#include "api/LayoutV2Api.inl"
#include "api/MediaV2Api.inl"
#include "api/TimingV2Api.inl"
#include "api/SequencerV2Api.inl"
#include "api/EffectsV2Api.inl"
#include "api/TimingAnalysisV2Api.inl"
#include "api/LegacySequenceCoreApi.inl"
#include "api/LegacyExportPackagingApi.inl"
#include "api/LegacyReadQueryApi.inl"

bool xLightsFrame::ProcessAutomation(std::vector<std::string> &paths,
                                     std::map<std::string, std::string> &params,
                                     const std::function<bool(const std::string &msg,
                                                              const std::string &jsonKey,
                                                              int responseCode,
                                                              bool msgIsJSON)> &sendResponse) {

    if (paths.size() == 0) {
        return sendResponse("No command", "msg", 503, false);
    }

    std::string cmd = paths[0];
    if (IsV2Command(params)) {
        auto requestIdIt = params.find("_REQUEST_ID");
        std::string requestId = requestIdIt == params.end() ? "" : requestIdIt->second;
        auto requireOpenSequence = [&]() -> std::optional<bool> {
            if (CurrentSeqXmlFile != nullptr) {
                return std::nullopt;
            }
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        };
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

        if (auto handled = automation::api::HandleSystemV2Command(cmd, params, requestId, sendResponse)) {
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

    if (auto handled = automation::api::HandleLegacySequenceCoreCommand(this, _sequenceElements, _promptBatchRenderIssues, _renderMode, xlightsFilename, cmd, paths, params, sendResponse)) {
        return *handled;
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
    } else if (auto handled = automation::api::HandleLegacyReadQueryCommand(
                   AllModels,
                   _outputManager,
                   _sequenceElements,
                   CurrentSeqXmlFile,
                   [&]() -> std::vector<std::string> {
                       std::vector<std::string> names;
                       auto allViews = GetViewsManager()->GetViews();
                       names.reserve(allViews.size());
                       for (auto* view : allViews) {
                           names.emplace_back(view->GetName());
                       }
                       return names;
                   },
                   cmd,
                   params,
                   sendResponse)) {
        return *handled;
    } else if (cmd == "saveLayout") {
        if (!layoutPanel->SaveEffects()) {
            return sendResponse("Failed to save layout.", "msg", 503, false);
        }

        if (!SaveNetworksFile()) {
            return sendResponse("Failed to controller tab.", "msg", 503, false);
        }

        return sendResponse("Layout and controller tab saved.", "msg", 200, false);

    } else if (cmd == "renderAll") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        auto ld = _lowDefinitionRender;
        auto highdef = params["highdef"];
        if (highdef == "true" && _lowDefinitionRender) {
            // override definition
            _lowDefinitionRender = false;
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::renderAll");
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::renderAll");
        }
        RenderAll();
        while (mRendering) {
            wxYield();
        }
        if (ld != _lowDefinitionRender) {
            _lowDefinitionRender = ld;
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::renderAll");
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::renderAll");
        }
        return sendResponse("Rendered.", "msg", 200, false);
    } else if (cmd == "batchRender") {
        wxArrayString files;

        auto ld = _lowDefinitionRender;
        auto highdef = params["highdef"];
        if (highdef == "true" && _lowDefinitionRender) {
            // override definition
            _lowDefinitionRender = false;
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::batchRender");
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::batchRender");
        }

        auto seqs = params["seqs_0"];
        int snum = 0;
        while (seqs != "") {
            auto seq = FindSequence(seqs);
            if (seq.empty()) {
                return sendResponse("Sequence not found '" + seq + "'", "msg", 503, false);
            }
            files.push_back(seq);
            snum++;
            seqs = params["seqs_" + std::to_string(snum)];
        }
        auto oldPrompt = _promptBatchRenderIssues;
        _promptBatchRenderIssues = ReadBool(params["promptIssues"]);

        _renderMode = true;
        _saveLowDefinitionRender = _lowDefinitionRender;
        OpenRenderAndSaveSequences(files, false);

        while (_renderMode) {
            wxYield();
        }

        _promptBatchRenderIssues = oldPrompt;
        if (ld != _lowDefinitionRender) {
            _lowDefinitionRender = ld;
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::batchRender");
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::batchRender");
        }
        return sendResponse("Sequence batch rendered.", "msg", 200, false);
    } else if (cmd == "uploadController") {
        auto ip = params["ip"];
        Controller* c = _outputManager.GetControllerWithIP(ip);
        if (c == nullptr) {
            return sendResponse("Controller not found '" + ip + "'", "msg", 503, false);
        }

        // ensure all start channels etc are up to date
        RecalcModels();

        bool res = true;
        auto caps = GetControllerCaps(c->GetName());
        if (caps != nullptr) {
            wxString message;
            if (caps->SupportsInputOnlyUpload()) {
                res = res && UploadInputToController(c, message);
            }
            res = res && UploadOutputToController(c, message);
        } else {
            res = false;
        }
        if (res) {
            return sendResponse("Uploaded to controller '" + ip + "'", "msg", 200, false);
        }
        return sendResponse("Upload to controller '" + ip + "' failed.", "msg", 503, false);
    } else if (cmd == "uploadFPPConfig") {
        auto ip = params["ip"];
        auto udp = params["udp"];
        auto models = params["models"];
        auto map = params["displayMap"];

        // discover the FPP instances
        auto instances = FPP::GetInstances(this, &_outputManager);

        FPP* fpp = nullptr;
        for (const auto& it : instances) {
            if (it->ipAddress == ip && it->fppType == FPP_TYPE::FPP) {
                fpp = it;
                break;
            }
        }
        if (fpp == nullptr) {
            return sendResponse("FPP not found '" + ip + "'.", "msg", 503, false);
        }

        if (udp == "all") {
            std::map<int, int> udpRanges;
            auto outputs = fpp->CreateUniverseFile(_outputManager.GetControllers(), false, &udpRanges);
            fpp->UploadUDPOut(outputs);
            fpp->SetRestartFlag();
        } else if (udp == "proxy") {
            fpp->UploadUDPOutputsForProxy(&_outputManager);
            fpp->SetRestartFlag();
        }

        if (models == "true" || models == "all") {
            auto memoryMaps = fpp->CreateModelMemoryMap(&AllModels, 0, std::numeric_limits<int32_t>::max());
            fpp->UploadModels(memoryMaps);
        } else if (udp == "local") {
            auto c = _outputManager.GetControllers(fpp->ipAddress);
            if (c.size() == 1) {
                auto const& memoryMaps = fpp->CreateModelMemoryMap(&AllModels, c.front()->GetStartChannel(), c.front()->GetEndChannel());
                fpp->UploadModels(memoryMaps);
            }
        }

        if (map == "true") {
            int pw, ph;
            GetLayoutPreview()->GetVirtualCanvasSize(pw, ph);
            std::map<std::string, std::string> virtualDisplayData;
            FPP::CreateVirtualDisplayMap(AllModels, AllObjects, pw, ph, virtualDisplayData);
            fpp->UploadDisplayMap(virtualDisplayData);
            // virtual display map  requires a restart
            fpp->SetRestartFlag(true);
        }

        //if restart flag is now set, restart and recheck range
        fpp->Restart(true);

        return sendResponse("Uploaded to FPP '" + ip + "'.", "msg", 200, false);
    } else if (cmd == "uploadSequence") {
        bool res = true;
        auto ip = params["ip"];
        auto media = ReadBool(params["media"]);
        auto format = params["format"];
        auto xsq = params["seq"];
        xsq = FindSequence(xsq);

        if (xsq.empty()) {
            return sendResponse("Sequence not found.", "msg", 503, false);
        }

        auto fseq = xLightsXmlFile::GetFSEQForXSQ(xsq, GetFseqDirectory());
        auto m2 = xLightsXmlFile::GetMediaForXSQ(xsq, CurrentDir, GetMediaFolders());

        if (!FileExists(fseq)) {
            return sendResponse("Unable to find sequence FSEQ file.", "msg", 503, false);
        }

        // discover the FPP instances
        auto instances = FPP::GetInstances(this, &_outputManager);

        FPP* fpp = nullptr;
        for (const auto& it : instances) {
            if (it->ipAddress == ip) {
                fpp = it;
                break;
            }
        }
        if (fpp == nullptr) {
            return sendResponse("Player " + ip + " not found.", "msg", 503, false);
        }

        int fseqType = 0;
        if (format == "v1") {
            fseqType = 0;
        } else if (format == "v2std") {
            fseqType = 1;
        } else if (format == "v2zlib") {
            fseqType = 5;
        } else if (format == "v2uncompressedsparse") {
            fseqType = 3;
        } else if (format == "v2uncompressed") {
            fseqType = 4;
        } else if (format == "v2stdsparse") {
            fseqType = 2;
        } else if (format == "v2zlibsparse") {
            fseqType = 6;
        }

        if (!media) {
            m2 = "";
        }

        FSEQFile* seq = FSEQFile::openFSEQFile(fseq);
        if (seq) {
            fpp->PrepareUploadSequence(seq, fseq, m2, fseqType);
            static const int FRAMES_TO_BUFFER = 50;
            std::vector<std::vector<uint8_t>> frames(FRAMES_TO_BUFFER);
            for (size_t x = 0; x < frames.size(); x++) {
                frames[x].resize(seq->getMaxChannel() + 1);
            }

            for (size_t frame = 0; frame < seq->getNumFrames(); frame++) {
                int lastBuffered = 0;
                size_t startFrame = frame;
                //Read a bunch of frames so each parallel thread has more info to work with before returning out here
                while (lastBuffered < FRAMES_TO_BUFFER && frame < seq->getNumFrames()) {
                    FSEQFile::FrameData* f = seq->getFrame(frame);
                    if (f != nullptr) {
                        if (!f->readFrame(&frames[lastBuffered][0], frames[lastBuffered].size())) {
                            //logger_base.error("FPPConnect FSEQ file corrupt.");
                            res = false;
                        }
                        delete f;
                    }
                    lastBuffered++;
                    frame++;
                }
                frame--;
                for (int x = 0; x < lastBuffered; x++) {
                    fpp->AddFrameToUpload(startFrame + x, &frames[x][0]);
                }
            }
            fpp->FinalizeUploadSequence();

            if (fpp->fppType == FPP_TYPE::FALCONV4V5) {
                // a falcon
                std::string proxy = "";
                auto c = _outputManager.GetControllers(fpp->ipAddress);
                if (c.size() == 1)
                    proxy = c.front()->GetFPPProxy();
                Falcon falcon(fpp->ipAddress, proxy);

                if (falcon.IsConnected()) {
                    falcon.UploadSequence(fpp->GetTempFile(), fseq, fpp->mode == "remote" ? "" : m2, nullptr);
                } else {
                    res = false;
                }
                fpp->ClearTempFile();
            }
            delete seq;
        } else {
            return sendResponse("Failed to generate FSEQ.", "msg", 503, false);
        }

        if (!res) {
            return sendResponse("Failed to upload.", "msg", 503, false);
        }
        return sendResponse("Sequence uploaded.", "msg", 200, false);
    } else if (cmd == "checkSequence") {
        auto seq = params["seq"];
        seq = FindSequence(seq);
        if (seq.empty()) {
            return sendResponse("Sequence not found.", "msg", 503, false);
        }
        auto file = OpenAndCheckSequence(seq);

        std::string response = wxString::Format("{\"msg\":\"Sequence checked.\",\"output\":\"%s\"}", JSONSafe(file));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "changeShowFolder") {
        auto shw = params["folder"];
        if (!wxDir::Exists(shw)) {
            return sendResponse("Folder does not exist.", "msg", 503, false);
        }

        auto force = ReadBool(params["force"]);
        if (CurrentSeqXmlFile != nullptr && mSavedChangeCount != _sequenceElements.GetChangeCount()) {
            if (force) {
                mSavedChangeCount = _sequenceElements.GetChangeCount();
            } else {
                return sendResponse("Sequence has unsaved changes.", "msg", 503, false);
            }
        }

        if (UnsavedRgbEffectsChanges) {
            if (force) {
                UnsavedRgbEffectsChanges = false;
            } else {
                return sendResponse("Layout has unsaved changes.", "msg", 503, false);
            }
        }

        if (UnsavedNetworkChanges) {
            if (force) {
                UnsavedNetworkChanges = false;
            } else {
                return sendResponse("Controller has unsaved changes.", "msg", 503, false);
            }
        }

        displayElementsPanel->SetSequenceElementsModelsViews(nullptr, nullptr, nullptr, nullptr, nullptr);
        layoutPanel->ClearUndo();
        SetDir(shw, true);

        return sendResponse("Show folder changed to " + shw + ".", "msg", 200, false);
    } else if (cmd == "openController") {
        auto ip = params["ip"];
        ::wxLaunchDefaultBrowser(ip);

        return sendResponse("Controller opened", "msg", 200, false);

    } else if (cmd == "openControllerProxy") {
        auto ip = params["ip"];
        auto controller = _outputManager.GetControllerWithIP(ip);

        if (controller == nullptr) {
            return "{\"res\":504,\"msg\":\"Controller not found.\"}";
        }

        auto proxy = controller->GetFPPProxy();

        if (proxy.empty()) {
            return "{\"res\":504,\"msg\":\"Controller has no proxy.\"}";
        }

        ::wxLaunchDefaultBrowser(proxy);

        return "{\"res\":200,\"msg\":\"Proxy opened.\"}";

    } else if (cmd == "closexLights") {
        auto force = ReadBool(params["force"]);
        if (CurrentSeqXmlFile != nullptr && mSavedChangeCount != _sequenceElements.GetChangeCount()) {
            if (force) {
                mSavedChangeCount = _sequenceElements.GetChangeCount();
            } else {
                return sendResponse("Sequence has unsaved changes.", "msg", 503, false);
            }
        }

        if (UnsavedRgbEffectsChanges) {
            if (force) {
                UnsavedRgbEffectsChanges = false;
            } else {
                return sendResponse("Layout has unsaved changes.", "msg", 503, false);
            }
        }

        if (UnsavedNetworkChanges) {
            if (force) {
                UnsavedNetworkChanges = false;
            } else {
                return sendResponse("Controller has unsaved changes.", "msg", 503, false);
            }
        }

        // Click on the File quit menu item
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, wxID_EXIT);
        wxPostEvent(this, evt);

        return sendResponse("xLights closed.", "msg", 200, false);
    } else if (cmd == "lightsOn") {
        EnableOutputs(true);
        return sendResponse("Lights on.", "msg", 200, false);
    } else if (cmd == "lightsOff") {
        DisableOutputs();
        return sendResponse("Lights off.", "msg", 200, false);
    } else if (cmd == "playJukebox") {
        int button = wxAtoi(params["button"]);
        if (CurrentSeqXmlFile != nullptr) {
            jukeboxPanel->PlayItem(button);
            return sendResponse("Played button " + std::to_string(button), "msg", 200, false);
        } else {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
     } else if (cmd == "jukeboxButtonTooltips" || cmd == "getJukeboxButtonTooltips") {
        if (CurrentSeqXmlFile != nullptr) {
            return sendResponse(jukeboxPanel->GetTooltipsJSON(), "tooltips", 200, true);
        } else {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
    } else if (cmd == "jukeboxButtonEffectPresent" || cmd == "getJukeboxButtonEffectPresent") {
        if (CurrentSeqXmlFile != nullptr) {
            return sendResponse(jukeboxPanel->GetEffectPresentJSON(), "effects", 200, true);
        } else {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
    } else if (cmd == "e131Tag" || cmd == "getE131Tag") {
        return sendResponse(E131Output::GetTag(), "tag", 200, false);
    } else if (cmd == "addEthernetController") {
        auto c = new ControllerEthernet(&_outputManager);
        //c->SetProtocol(params["protocol"]);
        c->SetIP(params["ip"]);
        c->SetId(1);
        c->EnsureUniqueId();
        c->SetName(params["name"]);
        auto const vendors = ControllerCaps::GetVendors(c->GetType());
        if (std::find(vendors.begin(), vendors.end(), params["vendor"]) != vendors.end()) {
            c->SetVendor(params["vendor"]);
            auto models = ControllerCaps::GetModels(c->GetType(), params["vendor"]);
            if (std::find(models.begin(), models.end(), params["model"]) != models.end()) {
                c->SetModel(params["model"]);
                auto variants = ControllerCaps::GetVariants(c->GetType(), params["vendor"], params["model"]);
                if (std::find(variants.begin(), variants.end(), params["variant"]) != variants.end()) {
                    c->SetVariant(params["variant"]);
                }
            }
        }
        
        _outputManager.AddController(c);
        _outputModelManager.AddASAPWork(OutputModelManager::WORK_NETWORK_CHANGE, "Automation:ADDETHERNET");
        _outputModelManager.AddASAPWork(OutputModelManager::WORK_NETWORK_CHANNELSCHANGE, "Automation:ADDETHERNET");
        _outputModelManager.AddASAPWork(OutputModelManager::WORK_UPDATE_NETWORK_LIST, "Automation:ADDETHERNET", nullptr, c);
        _outputModelManager.AddLayoutTabWork(OutputModelManager::WORK_CALCULATE_START_CHANNELS, "Automation:ADDETHERNET");
        return sendResponse("Added Ethernet Controller", "msg", 200, false);
    
    } else if (cmd == "runScript") {
        auto filename = params["filename"];
        if (filename.empty() || filename == "null" || !FileExists(filename)) {
            return sendResponse("Invalid Script Path.", "msg", 503, false);
        }

        LuaRunner runner(this);
        auto const worked = runner.Run_Script(filename, [](std::string const& m) {});
        if (worked) {
            std::string response = "{\"msg\":\"Script Was Successful.\"}";
            return sendResponse(response, "", 200, true);
        }
        return sendResponse("Script Failed", "msg", 503, true);
    } else if (cmd == "cloneModelEffects") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto target = params["target"];
        auto source = params["source"];
        auto erase = false;

        if (!params["eraseModel"].empty()) {
            erase = ReadBool(params["eraseModel"]);
        }
        auto const worked = CloneXLightsEffects(target, source, _sequenceElements, erase);
        mainSequencer->PanelEffectGrid->Refresh();
        std::string response = wxString::Format("{\"msg\":\"Model Effects Cloned.\",\"worked\":\"%s\"}", JSONSafe(toStr(worked)));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "addEffect") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto target = params["target"];
        auto effect = params["effect"];
        auto settings = params["settings"];
        auto palette = params["palette"];
        Element* to = _sequenceElements.GetElement(target);
        int startTime = 0;
        int endTime = CurrentSeqXmlFile->GetSequenceDurationMS();
        int layer = 0;

        if (!params["layer"].empty()) {
            layer = std::stoi(params["layer"]);
        }
        if (!params["startTime"].empty()) {
            startTime = std::stoi(params["startTime"]);
        }
        if (!params["endTime"].empty()) {
            endTime = std::stoi(params["endTime"]);
        }

        if (to == nullptr) {
            return sendResponse("target element doesn't exists.", "msg", 503, false);
        }
        _sequenceElements.get_undo_mgr().CreateUndoStep();
        while (to->GetEffectLayerCount() < layer + 1) {
            to->AddEffectLayer();
        }
        auto valid = to->GetEffectLayer(layer)->AddEffect(0, effect, settings, palette,
                                                          startTime, endTime, 0, false);
        mainSequencer->PanelEffectGrid->Refresh();
        std::string response = wxString::Format("{\"msg\":\"Added Effects.\",\"worked\":\"%s\"}", JSONSafe(toStr(valid != nullptr)));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "deleteAllAliases") {
        std::string models;
        bool deleted = false;
        for (auto m = (&AllModels)->begin(); m != (&AllModels)->end(); ++m) {
            bool ret = m->second->DeleteAllAliases();
            if (ret) {
                models += (deleted ? ", " : "") + JSONSafe(m->first);
                deleted = deleted || ret;
            }
        }
        if (deleted) {
            MarkEffectsFileDirty();
            return sendResponse("\"" + models + "\"", "models", 200, true);
        } else {
        	return sendResponse("No aliases found to delete.", "msg", 503, false);
		}
    } else if (cmd == "makeMaster") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        if (params["view"].empty()) {
            return sendResponse("No template view selected.", "msg", 504, false);
        }
        auto view = params["view"];

        displayElementsPanel->SelectView(view);

        // Click on the Make Master item
        displayElementsPanel->DoMakeMaster();
        std::string response = "{\"msg\":\"Master view updated.\"}";
        return sendResponse(response, "", 200, true);
    } else if (cmd == "cleanupFileLocations") {

        bool res = CleanupRGBEffectsFileLocations();

        if (CurrentSeqXmlFile != nullptr) {
            res = res && CleanupSequenceFileLocations();
        }

        if (res) {
            std::string response = "{\"msg\":\"Cleanup file locations.\",\"worked\":\"true\"}";
            return sendResponse(response, "", 200, true);
        }
        else
        {
            return sendResponse("Cleanup file locations failed.", "msg", 503, false);
        }

    } else if (cmd == "getEffectSettings") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int id = 0;
        int layer = 0;

        if (!params["id"].empty()) {
            id = std::stoi(params["id"]);
        }
        if (!params["layer"].empty()) {
            layer = std::stoi(params["layer"]);
        }
        auto const& model = params["model"];
        Element* ele = _sequenceElements.GetElement(model);
        if (ele == nullptr) {
            return sendResponse("target element doesn't exists.", "msg", 503, false);
        }
        auto* lay = ele->GetEffectLayer(layer);
        if (lay == nullptr) {
            return sendResponse("target layer doesn't exists.", "msg", 503, false);
        }
        auto* eff = lay->GetEffectFromID(id);
        if (eff != nullptr) {

            std::string json = "{\"name\":\"" + eff->GetEffectName() + "\"" +
                                ",\"settings\":" + eff->GetSettingsAsJSON() +
                               ",\"palette\":" + eff->GetPaletteAsJSON() +
                               ",\"startTime\":" + std::to_string(eff->GetStartTimeMS()) +
                               ",\"endTime\":" + std::to_string(eff->GetEndTimeMS()) +
                                ",\"selected\":" + std::to_string(eff->GetSelected()) + "}";
            return sendResponse(json, "", 200, true);
        }        
        return sendResponse("target effect doesn't exists.", "msg", 503, false);
    } else if (cmd == "setEffectSettings") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int id = 0;
        int layer = 0;

        if (!params["id"].empty()) {
            id = std::stoi(params["id"]);
        }
        if (!params["layer"].empty()) {
            layer = std::stoi(params["layer"]);
        }
        auto const& model = params["model"];
        Element* ele = _sequenceElements.GetElement(model);
        if (ele == nullptr) {
            return sendResponse("target element doesn't exists.", "msg", 503, false);
        }
        auto* lay = ele->GetEffectLayer(layer);
        if (lay == nullptr) {
            return sendResponse("target layer doesn't exists.", "msg", 503, false);
        }
        auto* eff = lay->GetEffectFromID(id);
        if (eff != nullptr) {

            if (!params["name"].empty()) {
                eff->SetEffectName(params["name"]);
            }
            if (!params["startTime"].empty()) {
                eff->SetStartTimeMS(std::stoi(params["startTime"]));
            }
            if (!params["endTime"].empty()) {
                eff->SetEndTimeMS(std::stoi(params["endTime"]));
            }
            if (!params["settings"].empty()) {
                eff->SetSettings(params["settings"], true , true);
            }
            if (!params["palette"].empty()) {
                eff->SetColourOnlyPalette(params["palette"], true);
            }
            mainSequencer->PanelEffectGrid->Refresh();
            mainSequencer->SelectEffect(eff);
            std::string response = wxString::Format("{\"msg\":\"Set Effect Settings.\",\"worked\":\"%s\"}", JSONSafe(toStr(eff != nullptr)));
            return sendResponse(response, "", 200, true);
        }
        return sendResponse("target effect doesn't exists.", "msg", 503, false);
    } else if (cmd == "importXLightsSequence") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto filename = params["filename"];
        if (filename == "" || filename == "null"|| !wxFile::Exists(filename)) {
            return sendResponse("Inport File not valid.", "msg", 503, false);
        }
        auto mapname = params["mapfile"];
        if (mapname == "" || mapname == "null" || !wxFile::Exists(mapname)) {
            return sendResponse("Mapping File no valid.", "msg", 503, false);
        }
        ImportXLights(wxFileName(filename), mapname);

        wxCommandEvent eventRowHeaderChanged(EVT_ROW_HEADINGS_CHANGED);
        wxPostEvent(this, eventRowHeaderChanged);
        mainSequencer->PanelEffectGrid->Refresh();
        
        std::string response = "{\"msg\":\"Imported XLights Sequence.\",\"worked\":\"true\"}";
        return sendResponse(response, "", 200, true);
    } else if (cmd == "getShowFolder") {
        return sendResponse(JSONSafe(showDirectory), "folder", 200, false);
    } else if (cmd == "setModelProperty") {
        auto model = params["model"];
        auto m = AllModels.GetModel(model);
        if (nullptr == m) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }
        auto propKey = params["key"];
        auto propData = params["data"];
        if (propKey.empty() || propData.empty()) {
            return sendResponse("Key or Data was empty.", "msg", 503, false);
        }
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
        std::string response = wxString::Format("{\"msg\":\"Set Model Property.\",\"worked\":\"%s\"}", JSONSafe(toStr(m != nullptr)));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "getFseqDirectory") {
        return sendResponse(JSONSafe(GetFseqDirectory()), "folder", 200, false);
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
 
/*

            

 } else if (cmd == "runDiscovery") {
     return "{\"res\":504,\"msg\":\"Not implemented.\"}";
     // TODO
 } else if (cmd == "exportModel") {
     return "{\"res\":504,\"msg\":\"Not implemented.\"}";
     // TODO
     // pass in name of the file to write to ... pass back the name of the file written to
 } else if (cmd == "exportModelAsCustom") {
     return "{\"res\":504,\"msg\":\"Not implemented.\"}";
     // TODO
     // pass in name of the file to write to ... pass back the name of the file written to

            } else if (cmd == "shiftAllEffects") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
                // pass in number of MS
            } else if (cmd == "shiftSelectedEffects") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "unselectEffects") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "selectEffectsOnModel") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "selectAllEffectsOnAllModels") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO

            } else if (cmd == "turnOnOutputToLights") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "turnOffOutputToLights") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "playSequence") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "printLayout") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "printWiring") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "exportLayoutImage") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "exportWiringImage") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "cleanupFileLocations") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "hinksPixExport") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "purgeDownloadCache") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // MEDIUM PRIORITY
                // TODO
            } else if (cmd == "purgeRenderCache") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // MEDIUM PRIORITY
                // TODO
            } else if (cmd == "convertSequence") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "prepareAudio") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "resetToDefaults") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // MEDIUM PRIORITY
                // TODO
            } else if (cmd == "resetWindowLayout") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // MEDIUM PRIORITY
                // TODO
            } else if (cmd == "setAudioVolume") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "setAudioSpeed") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "gotoZoom") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
            } else if (cmd == "importSequence") {
                return "{\"res\":504,\"msg\":\"Not implemented.\"}";
                // TODO
             else {
                return wxString::Format("{\"res\":504,\"msg\":\"Unknown command: '%s'.\"}", cmd);
            }
        }
    } else {
        return "{\"res\":504,\"msg\":\"Error parsing request.\"}";
    }
}
*/
