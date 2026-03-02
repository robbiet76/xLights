namespace automation::api {

static std::optional<bool> HandleLegacyReadQueryCommand(
    ModelManager& allModels,
    OutputManager& outputManager,
    SequenceElements& sequenceElements,
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<std::vector<std::string>()>& getViewNamesFn,
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

    if (cmd == "getModels") {
        std::string models;
        auto includeModels = getParam("models") != "false";
        auto includeGroups = getParam("groups") != "false";
        for (auto m = (&allModels)->begin(); m != (&allModels)->end(); ++m) {
            if (m->second->GetDisplayAs() == "ModelGroup" && !includeGroups) {
                continue;
            }
            if (m->second->GetDisplayAs() != "ModelGroup" && !includeModels) {
                continue;
            }
            models += "\"" + JSONSafe(m->first) + "\",";
        }
        if (!models.empty()) {
            models.pop_back();
        }
        models = "[" + models + "]";
        return sendResponse(models, "models", 200, true);
    }

    if (cmd == "getViews") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        std::string views;
        for (const auto& name : getViewNamesFn()) {
            views += "\"" + JSONSafe(name) + "\",";
        }
        if (!views.empty()) {
            views.pop_back();
        }
        views = "[" + views + "]";
        return sendResponse(views, "views", 200, true);
    }

    if (cmd == "getModel") {
        auto model = getParam("model");
        auto m = allModels.GetModel(model);
        if (m == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }
        auto json = m->GetAttributesAsJSON();
        return sendResponse(json, "model", 200, true);
    }

    if (cmd == "getControllers") {
        std::string controllers;
        for (const auto& it : outputManager.GetControllers()) {
            controllers += it->GetJSONData() + ",";
        }
        if (!controllers.empty()) {
            controllers.pop_back();
        }
        controllers = "[" + controllers + "]";
        return sendResponse(controllers, "controllers", 200, true);
    }

    if (cmd == "getControllerIPs") {
        std::string ipAddresses;
        for (const auto& it : outputManager.GetControllers()) {
            if (!it->GetIP().empty()) {
                ipAddresses += "\"" + JSONSafe(it->GetIP()) + "\",";
            }
        }
        if (!ipAddresses.empty()) {
            ipAddresses.pop_back();
        }
        ipAddresses = "[" + ipAddresses + "]";
        return sendResponse(ipAddresses, "controllers", 200, true);
    }

    if (cmd == "getControllerPortMap") {
        auto ip = getParam("ip");
        auto name = getParam("name");
        Controller* controller = nullptr;
        if (!name.empty()) {
            controller = outputManager.GetController(name);
        }
        if (!ip.empty()) {
            controller = outputManager.GetControllerWithIP(ip);
        }
        if (controller == nullptr) {
            return sendResponse("Controller not found.", "msg", 504, false);
        }
        UDController cud(controller, &outputManager, &allModels, false);
        auto json = cud.ExportAsJSON();
        return sendResponse(json, "controllerportmap", 200, true);
    }

    if (cmd == "getEffectIDs") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto model = getParam("model");
        Element* ele = sequenceElements.GetElement(model);
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
                ids.pop_back();
            }
            ids.insert(0, "[");
            ids.append("],");
            layers.append(ids);
        }
        if (layers.size() > 1) {
            layers.pop_back();
        }
        layers += "]";
        return sendResponse(layers, "effects", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
