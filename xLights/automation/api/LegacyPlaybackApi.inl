namespace automation::api {

static std::optional<bool> HandleLegacyPlaybackCommand(
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<void()>& enableOutputsFn,
    const std::function<void()>& disableOutputsFn,
    const std::function<void(int)>& playJukeboxItemFn,
    const std::function<std::string()>& getJukeboxTooltipsJsonFn,
    const std::function<std::string()>& getJukeboxEffectPresentJsonFn,
    const std::function<std::string()>& getE131TagFn,
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

    if (cmd == "lightsOn") {
        enableOutputsFn();
        return sendResponse("Lights on.", "msg", 200, false);
    }

    if (cmd == "lightsOff") {
        disableOutputsFn();
        return sendResponse("Lights off.", "msg", 200, false);
    }

    if (cmd == "playJukebox") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        int button = wxAtoi(getParam("button"));
        playJukeboxItemFn(button);
        return sendResponse("Played button " + std::to_string(button), "msg", 200, false);
    }

    if (cmd == "jukeboxButtonTooltips" || cmd == "getJukeboxButtonTooltips") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        return sendResponse(getJukeboxTooltipsJsonFn(), "tooltips", 200, true);
    }

    if (cmd == "jukeboxButtonEffectPresent" || cmd == "getJukeboxButtonEffectPresent") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        return sendResponse(getJukeboxEffectPresentJsonFn(), "effects", 200, true);
    }

    if (cmd == "e131Tag" || cmd == "getE131Tag") {
        return sendResponse(getE131TagFn(), "tag", 200, false);
    }

    return std::nullopt;
}

} // namespace automation::api
