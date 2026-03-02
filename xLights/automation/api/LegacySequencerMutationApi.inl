namespace automation::api {

static std::optional<bool> HandleLegacySequencerMutationCommand(
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<bool(const std::string&)>& runScriptFn,
    const std::function<bool(const std::string&, const std::string&, bool)>& cloneEffectsFn,
    const std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&, int, int, int)>& addEffectFn,
    const std::function<bool()>& cleanupRgbEffectsFn,
    const std::function<bool()>& cleanupSequenceFn,
    const std::function<bool(const std::string&, int, int, nlohmann::json&)>& getEffectDetailsFn,
    const std::function<bool(const std::string&, int, int, const std::map<std::string, std::string>&)>& setEffectDetailsFn,
    const std::function<bool(const std::string&, const std::string&)>& importXLightsSequenceFn,
    const std::function<void()>& refreshEffectGridFn,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    auto getParam = [&](const std::string& key) -> std::string {
        auto it = params.find(key);
        return it == params.end() ? "" : it->second;
    };

    if (cmd == "runScript") {
        auto filename = getParam("filename");
        if (filename.empty() || filename == "null" || !FileExists(filename)) {
            return sendResponse("Invalid Script Path.", "msg", 503, false);
        }
        if (runScriptFn(filename)) {
            return sendResponse("{\"msg\":\"Script Was Successful.\"}", "", 200, true);
        }
        return sendResponse("Script Failed", "msg", 503, true);
    }

    if (cmd == "cloneModelEffects") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto target = getParam("target");
        auto source = getParam("source");
        bool erase = !getParam("eraseModel").empty() ? ReadBool(getParam("eraseModel")) : false;
        auto worked = cloneEffectsFn(target, source, erase);
        refreshEffectGridFn();
        std::string response = wxString::Format("{\"msg\":\"Model Effects Cloned.\",\"worked\":\"%s\"}", JSONSafe(toStr(worked)));
        return sendResponse(response, "", 200, true);
    }

    if (cmd == "addEffect") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int layer = getParam("layer").empty() ? 0 : std::stoi(getParam("layer"));
        int startTime = getParam("startTime").empty() ? 0 : std::stoi(getParam("startTime"));
        int endTime = getParam("endTime").empty() ? currentSeqXmlFile->GetSequenceDurationMS() : std::stoi(getParam("endTime"));
        auto worked = addEffectFn(getParam("target"), getParam("effect"), getParam("settings"), getParam("palette"),
                                  layer, startTime, endTime);
        if (!worked) {
            return sendResponse("target element doesn't exists.", "msg", 503, false);
        }
        refreshEffectGridFn();
        std::string response = wxString::Format("{\"msg\":\"Added Effects.\",\"worked\":\"%s\"}", JSONSafe(toStr(worked)));
        return sendResponse(response, "", 200, true);
    }

    if (cmd == "cleanupFileLocations") {
        bool res = cleanupRgbEffectsFn();
        if (currentSeqXmlFile != nullptr) {
            res = res && cleanupSequenceFn();
        }
        if (res) {
            return sendResponse("{\"msg\":\"Cleanup file locations.\",\"worked\":\"true\"}", "", 200, true);
        }
        return sendResponse("Cleanup file locations failed.", "msg", 503, false);
    }

    if (cmd == "getEffectSettings") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int id = getParam("id").empty() ? 0 : std::stoi(getParam("id"));
        int layer = getParam("layer").empty() ? 0 : std::stoi(getParam("layer"));
        nlohmann::json data;
        if (!getEffectDetailsFn(getParam("model"), layer, id, data)) {
            return sendResponse("target effect doesn't exists.", "msg", 503, false);
        }
        return sendResponse(data.dump(), "", 200, true);
    }

    if (cmd == "setEffectSettings") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int id = getParam("id").empty() ? 0 : std::stoi(getParam("id"));
        int layer = getParam("layer").empty() ? 0 : std::stoi(getParam("layer"));
        if (!setEffectDetailsFn(getParam("model"), layer, id, params)) {
            return sendResponse("target effect doesn't exists.", "msg", 503, false);
        }
        refreshEffectGridFn();
        std::string response = "{\"msg\":\"Set Effect Settings.\",\"worked\":\"true\"}";
        return sendResponse(response, "", 200, true);
    }

    if (cmd == "importXLightsSequence") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto filename = getParam("filename");
        if (filename.empty() || filename == "null" || !wxFile::Exists(filename)) {
            return sendResponse("Import file not valid.", "msg", 503, false);
        }
        auto mapname = getParam("mapfile");
        if (mapname.empty() || mapname == "null" || !wxFile::Exists(mapname)) {
            return sendResponse("Mapping file not valid.", "msg", 503, false);
        }
        if (!importXLightsSequenceFn(filename, mapname)) {
            return sendResponse("Import failed.", "msg", 503, false);
        }
        refreshEffectGridFn();
        return sendResponse("{\"msg\":\"Imported XLights Sequence.\",\"worked\":\"true\"}", "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
