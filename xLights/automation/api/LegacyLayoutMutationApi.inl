namespace automation::api {

static std::optional<bool> HandleLegacyLayoutMutationCommand(
    ModelManager& allModels,
    OutputManager& outputManager,
    OutputModelManager& outputModelManager,
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<void()>& markEffectsDirtyFn,
    const std::function<void(const std::string&)>& selectViewAndMakeMasterFn,
    const std::function<bool(const std::string&, const std::string&, const std::string&)>& applyModelPropertyFn,
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

    if (cmd == "addEthernetController") {
        auto* controller = new ControllerEthernet(&outputManager);
        controller->SetIP(getParam("ip"));
        controller->SetId(1);
        controller->EnsureUniqueId();
        controller->SetName(getParam("name"));
        auto const vendors = ControllerCaps::GetVendors(controller->GetType());
        if (std::find(vendors.begin(), vendors.end(), getParam("vendor")) != vendors.end()) {
            controller->SetVendor(getParam("vendor"));
            auto models = ControllerCaps::GetModels(controller->GetType(), getParam("vendor"));
            if (std::find(models.begin(), models.end(), getParam("model")) != models.end()) {
                controller->SetModel(getParam("model"));
                auto variants = ControllerCaps::GetVariants(controller->GetType(), getParam("vendor"), getParam("model"));
                if (std::find(variants.begin(), variants.end(), getParam("variant")) != variants.end()) {
                    controller->SetVariant(getParam("variant"));
                }
            }
        }

        outputManager.AddController(controller);
        outputModelManager.AddASAPWork(OutputModelManager::WORK_NETWORK_CHANGE, "Automation:ADDETHERNET");
        outputModelManager.AddASAPWork(OutputModelManager::WORK_NETWORK_CHANNELSCHANGE, "Automation:ADDETHERNET");
        outputModelManager.AddASAPWork(OutputModelManager::WORK_UPDATE_NETWORK_LIST, "Automation:ADDETHERNET", nullptr, controller);
        outputModelManager.AddLayoutTabWork(OutputModelManager::WORK_CALCULATE_START_CHANNELS, "Automation:ADDETHERNET");
        return sendResponse("Added Ethernet Controller", "msg", 200, false);
    }

    if (cmd == "deleteAllAliases") {
        std::string models;
        bool deleted = false;
        for (auto m = (&allModels)->begin(); m != (&allModels)->end(); ++m) {
            bool ret = m->second->DeleteAllAliases();
            if (ret) {
                models += (deleted ? ", " : "") + JSONSafe(m->first);
                deleted = true;
            }
        }
        if (deleted) {
            markEffectsDirtyFn();
            return sendResponse("\"" + models + "\"", "models", 200, true);
        }
        return sendResponse("No aliases found to delete.", "msg", 503, false);
    }

    if (cmd == "makeMaster") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        auto view = getParam("view");
        if (view.empty()) {
            return sendResponse("No template view selected.", "msg", 504, false);
        }
        selectViewAndMakeMasterFn(view);
        return sendResponse("{\"msg\":\"Master view updated.\"}", "", 200, true);
    }

    if (cmd == "setModelProperty") {
        auto model = getParam("model");
        auto* m = allModels.GetModel(model);
        if (m == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }
        auto propKey = getParam("key");
        auto propData = getParam("data");
        if (propKey.empty() || propData.empty()) {
            return sendResponse("Key or Data was empty.", "msg", 503, false);
        }
        bool worked = applyModelPropertyFn(model, propKey, propData);
        std::string response = wxString::Format("{\"msg\":\"Set Model Property.\",\"worked\":\"%s\"}", JSONSafe(toStr(worked)));
        return sendResponse(response, "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
