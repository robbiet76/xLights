namespace automation::api {

static std::optional<bool> HandleLegacySystemControlCommand(
    SequenceElements& sequenceElements,
    xLightsXmlFile* currentSeqXmlFile,
    int& savedChangeCount,
    bool& unsavedRgbEffectsChanges,
    bool& unsavedNetworkChanges,
    const std::string& showDirectory,
    OutputManager& outputManager,
    const std::function<bool()>& saveLayoutEffectsFn,
    const std::function<bool()>& saveNetworksFileFn,
    const std::function<void()>& clearSequenceElementsAndUndoFn,
    const std::function<void(const std::string&)>& setShowDirFn,
    const std::function<void()>& closeXLightsFn,
    const std::function<void(const std::string&)>& launchBrowserFn,
    const std::function<std::string()>& getFseqDirectoryFn,
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
    auto handleUnsavedState = [&](bool force) -> std::optional<bool> {
        if (currentSeqXmlFile != nullptr && savedChangeCount != sequenceElements.GetChangeCount()) {
            if (force) {
                savedChangeCount = sequenceElements.GetChangeCount();
            } else {
                return sendResponse("Sequence has unsaved changes.", "msg", 503, false);
            }
        }
        if (unsavedRgbEffectsChanges) {
            if (force) {
                unsavedRgbEffectsChanges = false;
            } else {
                return sendResponse("Layout has unsaved changes.", "msg", 503, false);
            }
        }
        if (unsavedNetworkChanges) {
            if (force) {
                unsavedNetworkChanges = false;
            } else {
                return sendResponse("Controller has unsaved changes.", "msg", 503, false);
            }
        }
        return std::nullopt;
    };

    if (cmd == "saveLayout") {
        if (!saveLayoutEffectsFn()) {
            return sendResponse("Failed to save layout.", "msg", 503, false);
        }
        if (!saveNetworksFileFn()) {
            return sendResponse("Failed to controller tab.", "msg", 503, false);
        }
        return sendResponse("Layout and controller tab saved.", "msg", 200, false);
    }

    if (cmd == "changeShowFolder") {
        auto folder = getParam("folder");
        if (!wxDir::Exists(folder)) {
            return sendResponse("Folder does not exist.", "msg", 503, false);
        }

        auto force = ReadBool(getParam("force"));
        if (auto blocked = handleUnsavedState(force)) {
            return *blocked;
        }

        clearSequenceElementsAndUndoFn();
        setShowDirFn(folder);
        return sendResponse("Show folder changed to " + folder + ".", "msg", 200, false);
    }

    if (cmd == "openController") {
        launchBrowserFn(getParam("ip"));
        return sendResponse("Controller opened", "msg", 200, false);
    }

    if (cmd == "openControllerProxy") {
        auto ip = getParam("ip");
        auto* controller = outputManager.GetControllerWithIP(ip);
        if (controller == nullptr) {
            return sendResponse("Controller not found.", "msg", 504, false);
        }
        auto proxy = controller->GetFPPProxy();
        if (proxy.empty()) {
            return sendResponse("Controller has no proxy.", "msg", 504, false);
        }
        launchBrowserFn(proxy);
        return sendResponse("Proxy opened.", "msg", 200, false);
    }

    if (cmd == "closexLights") {
        auto force = ReadBool(getParam("force"));
        if (auto blocked = handleUnsavedState(force)) {
            return *blocked;
        }
        closeXLightsFn();
        return sendResponse("xLights closed.", "msg", 200, false);
    }

    if (cmd == "getShowFolder") {
        return sendResponse(JSONSafe(showDirectory), "folder", 200, false);
    }

    if (cmd == "getFseqDirectory") {
        return sendResponse(JSONSafe(getFseqDirectoryFn()), "folder", 200, false);
    }

    return std::nullopt;
}

} // namespace automation::api
