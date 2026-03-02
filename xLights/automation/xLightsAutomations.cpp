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

#include "LuaRunner.h"

#include <log4cpp/Category.hh>
#include <algorithm>
#include <cctype>
#include <cmath>

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
        "sequence.getOpen",
        "sequence.open",
        "sequence.create",
        "sequence.save",
        "sequence.close",
        "layout.getModels",
        "layout.getModel",
        "layout.getViews",
        "media.get",
        "media.set",
        "media.getMetadata",
        "timing.getTracks",
        "timing.createTrack",
        "timing.renameTrack",
        "timing.deleteTrack",
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

    auto groups = modelManager.GetGroupsContainingModel(model);
    nlohmann::json groupNames = nlohmann::json::array();
    for (const auto& group : groups) {
        groupNames.push_back(group);
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
                    if (value.is_array()) {
                        for (size_t i = 0; i < value.size(); i++) {
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

        if (cmd == "system.getCapabilities") {
            nlohmann::json data;
            data["apiVersions"] = { 2 };
            data["commands"] = GetV2Commands();

            data["features"] = {
                {"vampPluginsAvailable", false},
                {"remoteAudioAnalysisAvailable", true},
                {"lyricsSrtImportAvailable", true},
                {"songStructureDetectionAvailable", false}
            };
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "sequence.getOpen") {
            nlohmann::json data;
            if (CurrentSeqXmlFile == nullptr) {
                data["isOpen"] = false;
                data["sequence"] = nullptr;
            } else {
                data["isOpen"] = true;
                data["sequence"] = BuildV2SequenceData(CurrentSeqXmlFile);
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "sequence.open") {
            std::string file = ReadParamString(params, "file");
            bool force = ReadBool(ReadParamString(params, "force", "false"));
            bool promptIssues = ReadBool(ReadParamString(params, "promptIssues", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (file.empty() || file == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "file is required.", requestId), "", 422, true);
            }
            std::string seq = FindSequence(file);
            if (seq.empty()) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_FOUND", "Sequence not found.", requestId), "", 404, true);
            }

            if (CurrentSeqXmlFile != nullptr && !force) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "SEQUENCE_ALREADY_OPEN", "A sequence is already open.", requestId), "", 409, true);
            }

            if (dryRun) {
                nlohmann::json warnings = nlohmann::json::array();
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
                nlohmann::json data;
                data["file"] = seq;
                data["validated"] = true;
                return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
            }

            if (CurrentSeqXmlFile != nullptr) {
                if (mSavedChangeCount != _sequenceElements.GetChangeCount()) {
                    if (force) {
                        mSavedChangeCount = _sequenceElements.GetChangeCount();
                    } else {
                        return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Current sequence has unsaved changes.", requestId), "", 409, true);
                    }
                }
                AskCloseSequence();
            }

            auto oldPrompt = _promptBatchRenderIssues;
            auto oldRenderMode = _renderMode;
            if (!promptIssues) {
                _renderMode = true;
            }
            _promptBatchRenderIssues = promptIssues;
            OpenSequence(seq, nullptr);
            _promptBatchRenderIssues = oldPrompt;
            _renderMode = oldRenderMode;

            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(503, cmd, "OPEN_FAILED", "Failed to open sequence.", requestId), "", 503, true);
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceData(CurrentSeqXmlFile), requestId), "", 200, true);
        } else if (cmd == "sequence.create") {
            std::string mediaFile = ReadParamString(params, "mediaFile");
            int durationMs = ReadParamInt(params, "durationMs", 0);
            int frameMs = ReadParamInt(params, "frameMs", 0);
            std::string view = ReadParamString(params, "view");
            bool force = ReadBool(ReadParamString(params, "force", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (frameMs <= 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "frameMs must be > 0.", requestId), "", 422, true);
            }
            if ((mediaFile.empty() || mediaFile == "null") && durationMs <= 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "durationMs must be > 0 when mediaFile is not provided.", requestId), "", 422, true);
            }
            if (!mediaFile.empty() && mediaFile != "null") {
                wxFileName mediaPath(wxString::FromUTF8(mediaFile));
                if (!mediaPath.FileExists() || !mediaPath.IsFileReadable()) {
                    return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must exist and be readable.", requestId), "", 422, true);
                }
            }
            if (CurrentSeqXmlFile != nullptr && !force) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "SEQUENCE_ALREADY_OPEN", "A sequence is already open.", requestId), "", 409, true);
            }

            if (dryRun) {
                nlohmann::json warnings = nlohmann::json::array();
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
                nlohmann::json data;
                data["validated"] = true;
                data["frameMs"] = frameMs;
                if (!mediaFile.empty() && mediaFile != "null") {
                    data["mediaFile"] = mediaFile;
                } else {
                    data["durationMs"] = durationMs;
                }
                return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
            }

            if (CurrentSeqXmlFile != nullptr) {
                if (mSavedChangeCount != _sequenceElements.GetChangeCount()) {
                    if (force) {
                        mSavedChangeCount = _sequenceElements.GetChangeCount();
                    } else {
                        return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Current sequence has unsaved changes.", requestId), "", 409, true);
                    }
                }
                AskCloseSequence();
            }

            if (mediaFile == "null") {
                mediaFile.clear();
            }
            if (view == "null") {
                view.clear();
            }
            int durationSecs = durationMs > 0 ? durationMs / 1000 : 0;
            NewSequence(mediaFile, durationSecs * 1000, frameMs, view);
            EnableSequenceControls(true);

            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(503, cmd, "CREATE_FAILED", "Failed to create sequence.", requestId), "", 503, true);
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceData(CurrentSeqXmlFile), requestId), "", 200, true);
        } else if (cmd == "sequence.save") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string file = ReadParamString(params, "file");
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (file == "null") {
                file.clear();
            }

            if (dryRun) {
                nlohmann::json warnings = nlohmann::json::array();
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
                nlohmann::json data;
                data["saved"] = true;
                if (!file.empty()) {
                    data["file"] = file;
                } else if (!xlightsFilename.IsEmpty()) {
                    data["file"] = xlightsFilename.ToStdString();
                } else {
                    data["file"] = CurrentSeqXmlFile->GetFullPath().ToStdString();
                }
                return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
            }

            if (!file.empty()) {
                SaveAsSequence(file);
            } else {
                if (xlightsFilename.IsEmpty()) {
                    return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Saving unnamed sequence requires file.", requestId), "", 422, true);
                }
                SaveSequence();
            }

            nlohmann::json data;
            data["saved"] = true;
            if (!file.empty()) {
                data["file"] = file;
            } else if (!xlightsFilename.IsEmpty()) {
                data["file"] = xlightsFilename.ToStdString();
            } else {
                data["file"] = CurrentSeqXmlFile->GetFullPath().ToStdString();
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "sequence.close") {
            bool force = ReadBool(ReadParamString(params, "force", "false"));
            bool quiet = ReadBool(ReadParamString(params, "quiet", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (CurrentSeqXmlFile == nullptr) {
                if (quiet) {
                    nlohmann::json data;
                    data["closed"] = true;
                    return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
                }
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }

            if (mSavedChangeCount != _sequenceElements.GetChangeCount() && !force) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Sequence has unsaved changes.", requestId), "", 409, true);
            }

            if (dryRun) {
                nlohmann::json warnings = nlohmann::json::array();
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
                nlohmann::json data;
                data["closed"] = true;
                return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
            }

            if (mSavedChangeCount != _sequenceElements.GetChangeCount() && force) {
                mSavedChangeCount = _sequenceElements.GetChangeCount();
            }
            AskCloseSequence();

            nlohmann::json data;
            data["closed"] = true;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "layout.getModels") {
            nlohmann::json models = nlohmann::json::array();
            for (auto it = (&AllModels)->begin(); it != (&AllModels)->end(); ++it) {
                models.push_back(BuildV2ModelData(it->second, AllModels));
            }
            nlohmann::json data;
            data["models"] = models;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "layout.getModel") {
            std::string name = ReadParamString(params, "name");
            if (name.empty() || name == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name is required.", requestId), "", 422, true);
            }
            Model* model = AllModels.GetModel(name);
            if (model == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "MODEL_NOT_FOUND", "Model not found.", requestId), "", 404, true);
            }
            nlohmann::json data;
            data["model"] = BuildV2ModelData(model, AllModels);
            data["attributes"] = nlohmann::json::parse(model->GetAttributesAsJSON(), nullptr, false);
            if (data["attributes"].is_discarded()) {
                data["attributes"] = nlohmann::json::object();
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "layout.getViews") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            nlohmann::json views = nlohmann::json::array();
            auto allViews = GetViewsManager()->GetViews();
            for (auto* view : allViews) {
                nlohmann::json modelNames = nlohmann::json::array();
                auto models = view->GetModels();
                for (const auto& modelName : models) {
                    modelNames.push_back(modelName);
                }
                views.push_back({
                    {"name", view->GetName()},
                    {"models", modelNames}
                });
            }
            nlohmann::json data;
            data["views"] = views;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "media.get") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string mediaFile = CurrentSeqXmlFile->GetMediaFile().ToStdString();
            nlohmann::json data;
            data["mediaFile"] = mediaFile.empty() ? nlohmann::json(nullptr) : nlohmann::json(mediaFile);
            data["hasMedia"] = !mediaFile.empty();
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "media.set") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string mediaFile = ReadParamString(params, "mediaFile");
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
            if (mediaFile.empty() || mediaFile == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile is required.", requestId), "", 422, true);
            }
            wxFileName mediaPath(wxString::FromUTF8(mediaFile));
            if (!mediaPath.FileExists() || !mediaPath.IsFileReadable()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must exist and be readable.", requestId), "", 422, true);
            }

            std::string currentMedia = CurrentSeqXmlFile->GetMediaFile().ToStdString();
            bool updated = currentMedia != mediaFile;
            if (!dryRun) {
                CurrentSeqXmlFile->SetMediaFile(GetShowDirectory(), wxString::FromUTF8(mediaFile), true);
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["mediaFile"] = mediaFile;
            data["updated"] = updated;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "media.getMetadata") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string mediaFile = ReadParamString(params, "mediaFile");
            if (!mediaFile.empty() && mediaFile != "null" && mediaFile != CurrentSeqXmlFile->GetMediaFile().ToStdString()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase.", requestId), "", 422, true);
            }
            if (!CurrentSeqXmlFile->HasAudioMedia() || CurrentSeqXmlFile->GetMedia() == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId), "", 404, true);
            }

            auto* media = CurrentSeqXmlFile->GetMedia();
            long sampleRate = media->GetRate();
            long sampleCount = media->GetTrackSize();
            int channels = media->GetChannels();
            int durationMs = sampleRate > 0 ? static_cast<int>((sampleCount * 1000L) / sampleRate) : 0;

            nlohmann::json data;
            data["durationMs"] = durationMs;
            data["sampleRate"] = sampleRate;
            data["channels"] = channels;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "timing.getTracks") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            bool includeCounts = ReadBool(ReadParamString(params, "includeCounts", "false"));

            nlohmann::json tracks = nlohmann::json::array();
            int trackCount = _sequenceElements.GetNumberOfTimingElements();
            for (int i = 0; i < trackCount; i++) {
                TimingElement* track = _sequenceElements.GetTimingElement(i);
                if (track == nullptr) {
                    continue;
                }
                nlohmann::json entry;
                entry["name"] = track->GetName();
                entry["type"] = track->IsFixedTiming() ? "fixed" : "variable";
                if (includeCounts) {
                    int markCount = 0;
                    auto* layer0 = track->GetEffectLayer(0);
                    if (layer0 != nullptr) {
                        markCount = layer0->GetEffectCount();
                    }
                    entry["markCount"] = markCount;
                }
                tracks.push_back(entry);
            }

            nlohmann::json data;
            data["tracks"] = tracks;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "timing.createTrack") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string trackName = ReadParamString(params, "trackName");
            std::string trackType = ReadParamString(params, "trackType", "variable");
            bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
            bool addToAllViews = ReadBool(ReadParamString(params, "addToAllViews", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (trackName.empty() || trackName == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
            }
            if (trackType.empty() || trackType == "null") {
                trackType = "variable";
            }

            TimingElement* existingTrack = _sequenceElements.GetTimingElement(trackName);
            if (existingTrack != nullptr && !replaceIfExists) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
            }

            std::string action = existingTrack == nullptr ? "created" : "updated";
            if (!dryRun) {
                if (existingTrack != nullptr) {
                    _sequenceElements.DeleteElement(trackName);
                }
                std::string subType = trackType == "variable" ? "" : trackType;
                CurrentSeqXmlFile->AddNewTimingSection(trackName, this, subType);
                if (addToAllViews) {
                    _sequenceElements.AddTimingToAllViews(trackName);
                }
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["trackName"] = trackName;
            data["action"] = action;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.renameTrack") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string trackName = ReadParamString(params, "trackName");
            std::string newTrackName = ReadParamString(params, "newTrackName");
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (trackName.empty() || trackName == "null" || newTrackName.empty() || newTrackName == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName and newTrackName are required.", requestId), "", 422, true);
            }

            TimingElement* sourceTrack = _sequenceElements.GetTimingElement(trackName);
            if (sourceTrack == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
            }
            TimingElement* destTrack = _sequenceElements.GetTimingElement(newTrackName);
            if (destTrack != nullptr && newTrackName != trackName) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + newTrackName + "'.", requestId), "", 409, true);
            }

            if (!dryRun && newTrackName != trackName) {
                _sequenceElements.RenameTimingTrack(trackName, newTrackName);
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["trackName"] = trackName;
            data["newTrackName"] = newTrackName;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.deleteTrack") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            std::string trackName = ReadParamString(params, "trackName");
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (trackName.empty() || trackName == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
            }

            TimingElement* track = _sequenceElements.GetTimingElement(trackName);
            if (track == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
            }

            if (!dryRun) {
                _sequenceElements.DeleteElement(trackName);
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["deleted"] = true;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.listAnalysisPlugins") {
            std::string analysisUrl = ReadParamStringOrEnv(params, "analysisUrl", "XLIGHTS_ANALYSIS_URL");
            nlohmann::json warnings = nlohmann::json::array();
            if (analysisUrl.empty()) {
                warnings.push_back({
                    {"code", "REMOTE_URL_NOT_CONFIGURED"},
                    {"message", "Set analysisUrl or XLIGHTS_ANALYSIS_URL to enable remote analysis."}
                });
            }

            nlohmann::json data;
            data["providers"] = nlohmann::json::array({
                {{"id", "remote"}, {"name", "Remote Analysis Service"}, {"available", !analysisUrl.empty()}}
            });
            data["profiles"] = nlohmann::json::array({ "beats_v1", "bars_v1", "energy_v1", "structure_v1" });
            if (!analysisUrl.empty()) {
                data["analysisUrl"] = analysisUrl;
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.createFromAudio") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            if (!CurrentSeqXmlFile->HasAudioMedia() || CurrentSeqXmlFile->GetMedia() == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId), "", 404, true);
            }
            std::string trackName = ReadParamString(params, "trackName");
            std::string mediaFile = ReadParamString(params, "mediaFile");
            std::string analysisProvider = ReadParamString(params, "analysisProvider", "local");
            std::string analysisProfile = ReadParamString(params, "analysisProfile", "beats_v1");
            std::string analysisUrl = ReadParamStringOrEnv(params, "analysisUrl", "XLIGHTS_ANALYSIS_URL");
            bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
            bool addToAllViews = ReadBool(ReadParamString(params, "addToAllViews", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (trackName.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
            }
            if (!mediaFile.empty() && mediaFile != "null" && mediaFile != CurrentSeqXmlFile->GetMediaFile()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase." , requestId), "", 422, true);
            }

            TimingElement* existingTrack = _sequenceElements.GetTimingElement(trackName);
            if (existingTrack != nullptr && !replaceIfExists) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
            }

            if (analysisProvider != "remote") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "UNSUPPORTED_PROVIDER", "analysisProvider must be 'remote'.", requestId), "", 422, true);
            }
            if (analysisUrl.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "analysisUrl (or XLIGHTS_ANALYSIS_URL) is required for analysisProvider=remote.", requestId), "", 422, true);
            }

            nlohmann::json remoteRequest;
            remoteRequest["apiVersion"] = 2;
            remoteRequest["cmd"] = "timing.createFromAudio";
            remoteRequest["params"] = {
                {"trackName", trackName},
                {"mediaFile", CurrentSeqXmlFile->GetMediaFile()},
                {"showFolder", CurrentDir},
                {"analysisProfile", analysisProfile}
            };
            remoteRequest["options"] = {
                {"dryRun", dryRun},
                {"requestId", requestId},
                {"replaceIfExists", replaceIfExists},
                {"addToAllViews", addToAllViews}
            };

            int remoteStatus = 0;
            std::string remoteText = Curl::HTTPSPost(analysisUrl,
                                                     wxString::FromUTF8(remoteRequest.dump()),
                                                     "",
                                                     "",
                                                     "JSON",
                                                     300,
                                                     {},
                                                     &remoteStatus);
            if (remoteText.empty()) {
                return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis returned an empty response.", requestId), "", 502, true);
            }

            nlohmann::json remoteJson;
            try {
                remoteJson = nlohmann::json::parse(remoteText);
            } catch (const std::exception&) {
                return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis returned invalid JSON.", requestId), "", 502, true);
            }

            if (remoteJson.contains("error")) {
                std::string message = "Remote analysis error.";
                if (remoteJson["error"].is_object() && remoteJson["error"].contains("message")) {
                    message = remoteJson["error"]["message"].get<std::string>();
                }
                int status = 502;
                if (remoteJson.contains("res") && remoteJson["res"].is_number_integer()) {
                    status = remoteJson["res"].get<int>();
                } else if (remoteStatus >= 400) {
                    status = remoteStatus;
                }
                return sendResponse(BuildV2ErrorResponse(status, cmd, "REMOTE_ANALYSIS_FAILED", message, requestId), "", status, true);
            }

            std::vector<int> starts;
            std::vector<int> ends;
            std::vector<std::string> labels;
            if (!ParseRemoteSections(remoteJson, starts, ends, labels)) {
                return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis did not return timing sections.", requestId), "", 502, true);
            }

            if (!dryRun) {
                if (existingTrack != nullptr) {
                    _sequenceElements.DeleteElement(trackName);
                }
                CurrentSeqXmlFile->AddNewTimingSection(trackName, this, starts, ends, labels);
            }
            if (addToAllViews && !dryRun) {
                _sequenceElements.AddTimingToAllViews(trackName);
            }

            std::string action = existingTrack == nullptr ? "created" : "updated";
            nlohmann::json warnings = nlohmann::json::array();
            warnings.push_back({
                {"code", "REMOTE_PROVIDER"},
                {"message", "Timing marks were generated by remote analysis service."}
            });
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }

            nlohmann::json data;
            data["trackName"] = trackName;
            data["action"] = action;
            data["provider"] = "remote";
            data["analysisProfile"] = analysisProfile;
            data["markCount"] = static_cast<int>(starts.size());
            data["startMs"] = starts.front();
            data["endMs"] = ends.back();
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.getTrackSummary") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }

            std::string trackName = ReadParamString(params, "trackName");
            if (trackName.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
            }

            TimingElement* track = _sequenceElements.GetTimingElement(trackName);
            if (track == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
            }

            std::vector<int> starts;
            int startMs = 0;
            int endMs = 0;
            int markCount = 0;
            bool haveBounds = false;
            auto layer0 = track->GetEffectLayer(0);
            if (layer0 != nullptr) {
                auto effects = layer0->GetAllEffects();
                markCount = static_cast<int>(effects.size());
                for (auto* effect : effects) {
                    starts.push_back(effect->GetStartTimeMS());
                    if (!haveBounds || effect->GetStartTimeMS() < startMs) {
                        startMs = effect->GetStartTimeMS();
                        haveBounds = true;
                    }
                    if (!haveBounds || effect->GetEndTimeMS() > endMs) {
                        endMs = effect->GetEndTimeMS();
                    }
                }
            }
            std::sort(starts.begin(), starts.end());
            starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

            int minMs = 0;
            int maxMs = 0;
            int avgMs = 0;
            if (starts.size() > 1) {
                long long total = 0;
                for (size_t i = 1; i < starts.size(); i++) {
                    int interval = starts[i] - starts[i - 1];
                    if (i == 1 || interval < minMs) {
                        minMs = interval;
                    }
                    if (i == 1 || interval > maxMs) {
                        maxMs = interval;
                    }
                    total += interval;
                }
                avgMs = static_cast<int>(total / static_cast<long long>(starts.size() - 1));
            }

            int phrases = track->GetEffectLayerCount() > 0 && track->GetEffectLayer(0) != nullptr ? track->GetEffectLayer(0)->GetEffectCount() : 0;
            int words = track->GetEffectLayerCount() > 1 && track->GetEffectLayer(1) != nullptr ? track->GetEffectLayer(1)->GetEffectCount() : 0;
            int phonemes = track->GetEffectLayerCount() > 2 && track->GetEffectLayer(2) != nullptr ? track->GetEffectLayer(2)->GetEffectCount() : 0;

            nlohmann::json data;
            data["trackName"] = trackName;
            data["markCount"] = markCount;
            data["startMs"] = startMs;
            data["endMs"] = endMs;
            data["intervalStats"] = {
                {"minMs", minMs},
                {"maxMs", maxMs},
                {"avgMs", avgMs}
            };
            data["layers"] = {
                {"phrases", phrases},
                {"words", words},
                {"phonemes", phonemes}
            };
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        } else if (cmd == "timing.createBarsFromBeats") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }

            std::string sourceTrackName = ReadParamString(params, "sourceTrackName");
            std::string trackName = ReadParamString(params, "trackName");
            int beatsPerBar = ReadParamInt(params, "beatsPerBar", 4);
            bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

            if (sourceTrackName.empty() || trackName.empty() || beatsPerBar <= 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "sourceTrackName, trackName, and beatsPerBar>0 are required.", requestId), "", 422, true);
            }

            TimingElement* sourceTrack = _sequenceElements.GetTimingElement(sourceTrackName);
            if (sourceTrack == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Source timing track not found: '" + sourceTrackName + "'.", requestId), "", 404, true);
            }

            auto sourceLayer = sourceTrack->GetEffectLayer(0);
            if (sourceLayer == nullptr) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Source track has no timing marks.", requestId), "", 422, true);
            }

            auto sourceEffects = sourceLayer->GetAllEffects();
            std::vector<int> beatStarts;
            int sequenceEnd = CurrentSeqXmlFile->GetSequenceDurationMS();
            int lastAccepted = -1;
            for (auto* effect : sourceEffects) {
                int start = effect->GetStartTimeMS();
                if (start < 0 || start > sequenceEnd) {
                    continue;
                }
                if (lastAccepted >= 0 && start <= lastAccepted) {
                    continue;
                }
                beatStarts.push_back(start);
                lastAccepted = start;
            }

            if (beatStarts.size() < 2) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Insufficient valid beat marks in source track.", requestId), "", 422, true);
            }

            TimingElement* existingTrack = _sequenceElements.GetTimingElement(trackName);
            if (existingTrack != nullptr && !replaceIfExists) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
            }

            std::vector<int> starts;
            std::vector<int> ends;
            std::vector<std::string> labels;
            int downbeatCount = 0;
            for (size_t i = 0; i < beatStarts.size(); i += static_cast<size_t>(beatsPerBar)) {
                int start = beatStarts[i];
                int end = (i + static_cast<size_t>(beatsPerBar) < beatStarts.size()) ? beatStarts[i + static_cast<size_t>(beatsPerBar)] : sequenceEnd;
                if (end <= start) {
                    continue;
                }
                starts.push_back(start);
                ends.push_back(end);
                downbeatCount++;
                labels.push_back("Downbeat " + std::to_string(downbeatCount));
            }

            if (starts.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "No valid bar ranges could be generated.", requestId), "", 422, true);
            }

            std::string action = existingTrack == nullptr ? "created" : "updated";
            if (!dryRun) {
                if (existingTrack != nullptr) {
                    _sequenceElements.DeleteElement(trackName);
                }
                CurrentSeqXmlFile->AddNewTimingSection(trackName, this, starts, ends, labels);
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["trackName"] = trackName;
            data["action"] = action;
            data["sourceTrackName"] = sourceTrackName;
            data["beatsPerBar"] = beatsPerBar;
            data["barCount"] = static_cast<int>(starts.size());
            data["downbeatCount"] = downbeatCount;
            data["startMs"] = starts.front();
            data["endMs"] = ends.back();
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        } else if (cmd == "timing.createEnergySections") {
            if (CurrentSeqXmlFile == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
            }
            if (!CurrentSeqXmlFile->HasAudioMedia() || CurrentSeqXmlFile->GetMedia() == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId), "", 404, true);
            }

            std::string trackName = ReadParamString(params, "trackName");
            std::string mediaFile = ReadParamString(params, "mediaFile");
            bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
            int smoothingMs = ReadParamInt(params, "smoothingMs", 0);
            bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
            std::vector<std::string> levels = ReadParamArray(params, "levels");
            if (levels.empty()) {
                levels = { "low", "medium", "high" };
            }

            if (trackName.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
            }
            if (!mediaFile.empty() && mediaFile != "null" && mediaFile != CurrentSeqXmlFile->GetMediaFile()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase." , requestId), "", 422, true);
            }
            if (smoothingMs < 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "smoothingMs must be >= 0.", requestId), "", 422, true);
            }

            std::vector<std::string> uniqueLevels;
            for (const auto& level : levels) {
                if (level.empty()) {
                    continue;
                }
                if (std::find(uniqueLevels.begin(), uniqueLevels.end(), level) == uniqueLevels.end()) {
                    uniqueLevels.push_back(level);
                }
            }
            if (uniqueLevels.size() < 2) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "levels must contain at least two distinct labels.", requestId), "", 422, true);
            }

            TimingElement* existingTrack = _sequenceElements.GetTimingElement(trackName);
            if (existingTrack != nullptr && !replaceIfExists) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
            }

            int duration = CurrentSeqXmlFile->GetSequenceDurationMS();
            if (duration <= 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Sequence duration must be greater than 0.", requestId), "", 422, true);
            }

            auto* media = CurrentSeqXmlFile->GetMedia();
            long sampleRate = media->GetRate();
            long sampleCount = media->GetTrackSize();
            if (sampleRate <= 0 || sampleCount <= 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to analyze audio for energy sections.", requestId), "", 422, true);
            }

            int windowMs = smoothingMs > 0 ? smoothingMs : 500;
            if (windowMs < 50) {
                windowMs = 50;
            }
            long windowSamples = (sampleRate * windowMs) / 1000;
            if (windowSamples < 1) {
                windowSamples = 1;
            }
            long hopSamples = windowSamples / 2;
            if (hopSamples < 1) {
                hopSamples = 1;
            }

            std::vector<double> energies;
            std::vector<int> frameStartsMs;
            std::vector<int> frameEndsMs;
            for (long startSample = 0; startSample < sampleCount; startSample += hopSamples) {
                long endSample = std::min(sampleCount, startSample + windowSamples);
                if (endSample <= startSample) {
                    continue;
                }

                double sumSq = 0.0;
                for (long i = startSample; i < endSample; i++) {
                    double l = media->GetFilteredLeftData(i);
                    double sample = l;
                    if (media->GetChannels() > 1) {
                        double r = media->GetFilteredRightData(i);
                        sample = 0.5 * (l + r);
                    }
                    sumSq += sample * sample;
                }

                double n = static_cast<double>(endSample - startSample);
                energies.push_back(std::sqrt(sumSq / n));
                frameStartsMs.push_back(static_cast<int>((startSample * 1000) / sampleRate));
                frameEndsMs.push_back(static_cast<int>((endSample * 1000) / sampleRate));
            }

            if (energies.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to derive energy frames from audio.", requestId), "", 422, true);
            }

            double minEnergy = energies[0];
            double maxEnergy = energies[0];
            for (double e : energies) {
                if (e < minEnergy) {
                    minEnergy = e;
                }
                if (e > maxEnergy) {
                    maxEnergy = e;
                }
            }

            std::vector<int> frameBins;
            std::vector<double> frameConfidence;
            int bins = static_cast<int>(uniqueLevels.size());
            for (double e : energies) {
                double norm = (maxEnergy > minEnergy) ? ((e - minEnergy) / (maxEnergy - minEnergy)) : 0.5;
                int idx = static_cast<int>(norm * bins);
                if (idx >= bins) {
                    idx = bins - 1;
                }
                if (idx < 0) {
                    idx = 0;
                }
                frameBins.push_back(idx);

                double center = (static_cast<double>(idx) + 0.5) / static_cast<double>(bins);
                double conf = 1.0 - std::min(1.0, std::abs(norm - center) * 2.0);
                frameConfidence.push_back(conf);
            }

            std::vector<int> starts;
            std::vector<int> ends;
            std::vector<std::string> labels;
            std::vector<double> confidences;
            size_t segmentStart = 0;
            while (segmentStart < frameBins.size()) {
                int currentBin = frameBins[segmentStart];
                size_t segmentEnd = segmentStart + 1;
                double confTotal = frameConfidence[segmentStart];
                while (segmentEnd < frameBins.size() && frameBins[segmentEnd] == currentBin) {
                    confTotal += frameConfidence[segmentEnd];
                    segmentEnd++;
                }

                int sectionStart = frameStartsMs[segmentStart];
                int sectionEnd = frameEndsMs[segmentEnd - 1];
                if (sectionEnd > sectionStart) {
                    starts.push_back(sectionStart);
                    ends.push_back(sectionEnd);
                    labels.push_back(uniqueLevels[currentBin]);
                    confidences.push_back(confTotal / static_cast<double>(segmentEnd - segmentStart));
                }
                segmentStart = segmentEnd;
            }

            if (starts.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to derive valid energy sections.", requestId), "", 422, true);
            }

            // Normalize to full-song contiguous coverage for deterministic machine consumers.
            starts.front() = 0;
            for (size_t i = 1; i < starts.size(); i++) {
                starts[i] = ends[i - 1];
            }
            ends.back() = duration;

            std::string action = existingTrack == nullptr ? "created" : "updated";
            if (!dryRun) {
                if (existingTrack != nullptr) {
                    _sequenceElements.DeleteElement(trackName);
                }
                CurrentSeqXmlFile->AddNewTimingSection(trackName, this, starts, ends, labels);
            }

            nlohmann::json sections = nlohmann::json::array();
            for (size_t i = 0; i < starts.size(); i++) {
                double confidence = std::max(0.0, std::min(1.0, confidences[i]));
                sections.push_back({
                    {"label", labels[i]},
                    {"startMs", starts[i]},
                    {"endMs", ends[i]},
                    {"confidence", confidence}
                });
            }

            nlohmann::json warnings = nlohmann::json::array();
            if (dryRun) {
                warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
            }
            nlohmann::json data;
            data["trackName"] = trackName;
            data["action"] = action;
            data["sectionCount"] = static_cast<int>(sections.size());
            data["sections"] = sections;
            int coverageMs = 0;
            for (size_t i = 0; i < starts.size(); i++) {
                coverageMs += std::max(0, ends[i] - starts[i]);
            }
            data["coverageMs"] = coverageMs;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        return sendResponse(BuildV2ErrorResponse(404, cmd, "UNKNOWN_COMMAND", "Unknown command: '" + cmd + "'.", requestId), "", 404, true);
    }

    if (cmd == "getVersion") {
        return sendResponse(GetDisplayVersionString(), "version", 200, false);
    } else if (cmd == "openSequence" || cmd == "getOpenSequence" || cmd == "loadSequence") {
        wxString fname = "";
        if (paths.size() > 1) {
            fname = wxURI::Unescape(paths[1]);
        }
        bool force = false;
        bool prompt = false;
        
        if (params["_METHOD"] == "POST" && !params["_DATA"].empty()) {
            wxString data = params["_DATA"];
            try {
                nlohmann::json val = nlohmann::json::parse(data.ToStdString());
                // wxJSONReader reader;
                //  if (reader.Parse(data, &val) == 0)
                {
                    fname = val["seq"].get<std::string>();
                    if (val.contains("promptIssues")) {
                        prompt = ReadBool(params["promptIssues"]);
                    }
                    if (val.contains("force")) {
                        force = ReadBool(params["force"]);
                    }
                }
            } catch (const std::exception& e) {
                return sendResponse(wxString::Format("Failed to parse JSON data: %s", e.what()), "msg", 503, false);
            }
        } else {
            if (params["seq"] != "") {
                fname = params["seq"];
            }
            prompt = ReadBool(params["promptIssues"]);
            force = ReadBool(params["force"]);
        }
        if (fname.empty()) {
            if (CurrentSeqXmlFile != nullptr) {
                std::string response = wxString::Format("{\"seq\":\"%s\",\"fullseq\":\"%s\",\"media\":\"%s\",\"len\":%u,\"framems\":%u}",
                                                        JSONSafe(CurrentSeqXmlFile->GetName()),
                                                        JSONSafe(CurrentSeqXmlFile->GetFullPath()),
                                                        JSONSafe(CurrentSeqXmlFile->GetMediaFile()),
                                                        CurrentSeqXmlFile->GetSequenceDurationMS(),
                                                        CurrentSeqXmlFile->GetFrameMS());

                return sendResponse(response, "", 200, true);
            } else {
                return sendResponse("Sequence not open.", "msg", 503, false);
            }
        } else {
            std::string seq = FindSequence(fname);
            if (seq.empty()) {
                return sendResponse("Sequence not found.", "msg", 503, false);
            }
            if (CurrentSeqXmlFile != nullptr && force) {
                return sendResponse("Sequence already open.", "msg", 503, false);
            }
            auto oldPrompt = _promptBatchRenderIssues;
            auto oldRenderMode = _renderMode;
            if (!prompt) _renderMode = true;
            _promptBatchRenderIssues = prompt; // off by default
            OpenSequence(seq, nullptr);
            _promptBatchRenderIssues = oldPrompt;
            _renderMode = oldRenderMode;
            std::string response = wxString::Format("{\"seq\":\"%s\",\"fullseq\":\"%s\",\"media\":\"%s\",\"len\":%u,\"framems\":%u}",
                                                    JSONSafe(CurrentSeqXmlFile->GetName()),
                                                    JSONSafe(CurrentSeqXmlFile->GetFullPath()),
                                                    JSONSafe(CurrentSeqXmlFile->GetMediaFile()),
                                                    CurrentSeqXmlFile->GetSequenceDurationMS(),
                                                    CurrentSeqXmlFile->GetFrameMS());

            return sendResponse(response, "", 200, true);
        }
    } else if (cmd == "closeSequence") {
        if (CurrentSeqXmlFile == nullptr) {
            if (!ReadBool(params["quiet"])) {
                return sendResponse("Sequence not open.", "msg", 503, false);
            }
            return sendResponse("Sequence closed.", "msg", 200, false);
        }

        auto force = ReadBool(params["force"]);
        if (mSavedChangeCount != _sequenceElements.GetChangeCount()) {
            if (force) {
                mSavedChangeCount = _sequenceElements.GetChangeCount();
            } else {
                return sendResponse("Sequence has unsaved changes.", "msg", 504, false);
            }
        }

        AskCloseSequence();
        return sendResponse("Sequence closed.", "msg", 200, false);
    } else if (cmd == "saveLayout") {
        if (!layoutPanel->SaveEffects()) {
            return sendResponse("Failed to save layout.", "msg", 503, false);
        }

        if (!SaveNetworksFile()) {
            return sendResponse("Failed to controller tab.", "msg", 503, false);
        }

        return sendResponse("Layout and controller tab saved.", "msg", 200, false);

    } else if (cmd == "newSequence") {
        if (CurrentSeqXmlFile != nullptr && !ReadBool(params["force"])) {
            return sendResponse("Sequence already open.", "msg", 503, false);
        }

        auto media = params["mediaFile"];
        if (media == "null")
            media = "";
        auto duration = wxAtoi(params["durationSecs"]) * 1000;

        uint32_t frameMS = wxAtoi(params["frameMS"]); // this will be 0 if "null" so ok

        std::string view = params["view"];
        if (view == "null")
            view = "";

        NewSequence(media, duration, frameMS, view);
        EnableSequenceControls(true);
        return sendResponse("Sequence created.", "msg", 200, false);
    } else if (cmd == "saveSequence") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        auto seq = params["seq"];

        if (seq != "" && seq != "null") {
            SaveAsSequence(seq);
        } else {
            if (xlightsFilename.IsEmpty()) {
                return sendResponse("Saving unnamed sequence needs a name to be sent.", "msg", 503, false);
            }
            SaveSequence();
        }
        return sendResponse("Sequence Saved.", "msg", 200, false);
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

    } else if (cmd == "exportModelsCSV") {
        auto filename = params["filename"];
        if (filename == "" || filename == "null") {
            wxFileName f;
            f.AssignTempFileName("Models_");
            filename = f.GetFullPath();
        }

        ExportModels(filename);

        std::string response = wxString::Format("{\"msg\":\"Models Exported.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "exportModel") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto model = params["model"];
        if (AllModels.GetModel(model) == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }

        auto filename = params["filename"];
        auto format = params["format"];

        if (format == "lsp") {
            format = "LSP";
        } else if (format == "lorclipboard") {
            format = "Lcb";
        } else if (format == "lorclipboards5") {
            format = "LcbS5";
        } else if (format == "vixenroutine") {
            format = "Vir";
        } else if (format == "hls") {
            format = "HLS";
        } else if (format == "eseq") {
            format = "FPP";
        } else if (format == "eseqcompressed") {
            format = "FPPCompressed";
        } else if (format == "avicompressed" || format == "mp4compressed") {
            format = "Com";
        } else if (format == "aviuncompressed" || format == "mp4uncompressed") {
            format = "Unc";
        } else if (format == "minleon") {
            format = "Min";
        } else if (format == "gif") {
            format = "GIF";
        } else {
            return sendResponse("Unknown format.", "msg", 503, false);
        }

        if (DoExportModel(0, 0, model, filename, format, false)) {
            return sendResponse("Model exported.", "msg", 200, false);
        } else {
            return sendResponse("Failed to export.", "msg", 503, false);
        }
    } else if (cmd == "exportModelWithRender") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto ld = _lowDefinitionRender;
        auto highdef = params["highdef"];
        auto model = params["model"];

        if (AllModels.GetModel(model) == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }

        if (highdef == "true" && _lowDefinitionRender) {
            // override definition
            _lowDefinitionRender = false;
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_ALLMODELS, "Automation::exportModelWithRender");
            _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
        }

        auto filename = params["filename"];
        auto format = params["format"];

        if (format == "lsp") {
            format = "LSP";
        } else if (format == "lorclipboard") {
            format = "Lcb";
        } else if (format == "lorclipboards5") {
            format = "LcbS5";
        } else if (format == "vixenroutine") {
            format = "Vir";
        } else if (format == "hls") {
            format = "HLS";
        } else if (format == "eseq") {
            format = "FPP";
        } else if (format == "eseqcompressed") {
            format = "FPPCompressed";
        } else if (format == "avicompressed" || format == "mp4compressed") {
            format = "Com";
        } else if (format == "aviuncompressed" || format == "mp4uncompressed") {
            format = "Unc";
        } else if (format == "minleon") {
            format = "Min";
        } else if (format == "gif") {
            format = "GIF";
        } else {
            return sendResponse("Unknown format.", "msg", 503, false);
        }

        if (DoExportModel(0, 0, model, filename, format, true)) {
            if (ld != _lowDefinitionRender) {
                _lowDefinitionRender = ld;
                _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_ALLMODELS, "Automation::exportModelWithRender");  // Restore the models back to prior
                _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
            }
            return sendResponse("Model exported.", "msg", 200, false);
        } else {
            if (ld != _lowDefinitionRender) {
                _lowDefinitionRender = ld;
                _outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::exportModelWithRender");
                _outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
            }
            return sendResponse("Failed to export.", "msg", 503, false);
        }
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
    
    } else if (cmd == "packageSequence") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto const filename = PackageSequence(false);
        std::string response = wxString::Format("{\"msg\":\"Sequence Packaged.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);
    } else if (cmd == "packageLogFiles") {
        auto const filename = PackageDebugFiles(false);
        std::string response = wxString::Format("{\"msg\":\"Log Files Packaged.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);

    } else if (cmd == "exportVideoPreview") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto filename = params["filename"];
        if (filename == "" || filename == "null") {
            filename = CurrentDir + wxFileName::GetPathSeparator() + CurrentSeqXmlFile->GetName() + ".mp4";
        }
        auto const worked = ExportVideoPreview(filename);
        if (worked) {
            std::string response = wxString::Format("{\"msg\":\"Export Video Preview.\",\"output\":\"%s\"}", JSONSafe(filename));
            return sendResponse(response, "", 200, true);
        }        
        return sendResponse("Export Video Preview Failed", "msg", 503, true);
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
    } else if (cmd == "getModels") {
        std::string models;
        bool includeModels {true};
        bool includeGroups {true};
        auto sModels = params["models"];
        auto sGroups = params["groups"];
        includeModels = sModels != "false";
        includeGroups = sGroups != "false";
        for (auto m = (&AllModels)->begin(); m != (&AllModels)->end(); ++m) {
            if (m->second->GetDisplayAs() == "ModelGroup" && !includeGroups) {
                continue;
            }
            if (m->second->GetDisplayAs() != "ModelGroup" && !includeModels) {
                continue;
            }
            models += "\"" + JSONSafe(m->first) + "\",";
        }
        if (!models.empty()) {
            models.pop_back();//remove last comma
        }
        models = "[" + models + "]";
        return sendResponse(models, "models", 200, true);
        
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
    } else if (cmd == "getViews") {
        std::string views; 
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        auto AllViews = GetViewsManager()->GetViews();
        for (auto it = AllViews.begin(); it != AllViews.end(); ++it) {
            views += "\"" + JSONSafe((*it)->GetName()) + "\",";            
        }
        if (!views.empty()) {
            views.pop_back(); // remove last comma
        }
        views = "[" + views + "]";
        return sendResponse(views, "views", 200, true);

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
    } else if (cmd == "getModel") {
        auto model = params["model"];
        auto m = AllModels.GetModel(model);
        if (nullptr == m) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }
        auto json = m->GetAttributesAsJSON();
        return sendResponse(json, "model", 200, true);
        
    } else if (cmd == "getControllers") {
        std::string controllers;
        for (const auto& it : _outputManager.GetControllers()) {
            std::string json = it->GetJSONData() + ",";
            controllers += json;
        }
        if (!controllers.empty()) {
            controllers.pop_back();//remove last comma
        }
        controllers = "[" + controllers + "]";
        return sendResponse(controllers, "controllers", 200, true);
    } else if (cmd == "getControllerIPs") {
        std::string ipAddresses;
        for (const auto& it : _outputManager.GetControllers()) {
            if (!it->GetIP().empty()) {
                ipAddresses += "\"" + JSONSafe(it->GetIP()) + "\",";
            }
        }
        if (!ipAddresses.empty()) {
            ipAddresses.pop_back();//remove last comma
        }
        ipAddresses = "[" + ipAddresses + "]";
        return sendResponse(ipAddresses, "controllers", 200, true);
    } else if (cmd == "getControllerPortMap") {
        auto ip = params["ip"];
        auto name = params["name"];
        Controller* controller {nullptr};
        if (!name.empty()) {
            controller = _outputManager.GetController(name);
        }
        if (!ip.empty()) {
            controller = _outputManager.GetControllerWithIP(ip);
        }
        if (controller == nullptr) {
            return "{\"res\":504,\"msg\":\"Controller not found.\"}";
        }
        UDController cud(controller, &_outputManager, &AllModels, false);
        auto json = cud.ExportAsJSON();
        return sendResponse(json, "controllerportmap", 200, true);
    } else if (cmd == "getEffectIDs") {
        if (CurrentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto model = params["model"];
        Element* ele = _sequenceElements.GetElement(model);
        if (ele == nullptr) {
            return sendResponse("target element doesn't exists.", "msg", 503, false);
        }
        std::string layers = "[";
        for (int i = 0; i < ele->GetEffectLayerCount(); ++i) {
            std::string ids;
            auto effects = ele->GetEffectLayer(i)->GetAllEffects();
            for (auto* eff : effects) {
                ids += "\"" + std::to_string(eff->GetID()) + "\",";
            }
            if (!ids.empty()) {
                ids.pop_back(); // remove last comma
            }
            ids.insert(0, "[");
            ids.append("],");
            layers.append(ids);
        }
        layers.pop_back(); // remove last comma
        layers += "]";
        return sendResponse(layers, "effects", 200, true);
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
