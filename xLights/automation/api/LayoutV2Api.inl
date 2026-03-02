namespace automation::api {

static std::optional<bool> HandleLayoutV2Command(
    xLightsFrame* frame,
    ModelManager& allModels,
    SequenceElements& sequenceElements,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "layout.getModels") {
        nlohmann::json models = nlohmann::json::array();
        for (auto it = (&allModels)->begin(); it != (&allModels)->end(); ++it) {
            models.push_back(BuildV2ModelData(it->second, allModels));
        }
        nlohmann::json data;
        data["models"] = models;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getModel") {
        std::string name = ReadParamString(params, "name");
        if (name.empty() || name == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name is required.", requestId), "", 422, true);
        }
        Model* model = allModels.GetModel(name);
        if (model == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MODEL_NOT_FOUND", "Model not found.", requestId), "", 404, true);
        }
        nlohmann::json data;
        data["model"] = BuildV2ModelData(model, allModels);
        data["attributes"] = nlohmann::json::parse(model->GetAttributesAsJSON(), nullptr, false);
        if (data["attributes"].is_discarded()) {
            data["attributes"] = nlohmann::json::object();
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getViews") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        nlohmann::json views = nlohmann::json::array();
        auto allViews = frame->GetViewsManager()->GetViews();
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
    }

    if (cmd == "layout.getDisplayElements") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        nlohmann::json data;
        data["elements"] = BuildLayoutDisplayElementsData(sequenceElements);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
