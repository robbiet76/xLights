namespace automation::api {

static nlohmann::json BuildModelGeometryData(Model* model, const ModelManager& allModels) {
    const auto& location = model->GetModelScreenLocation();
    glm::vec3 position = location.GetWorldPosition();
    glm::vec3 rotation = location.GetRotation();
    glm::vec3 scale = location.GetScaleMatrix();

    nlohmann::json data;
    data["name"] = model->GetName();
    data["type"] = model->GetDisplayAs();
    data["layoutGroup"] = model->GetLayoutGroup();
    data["groupNames"] = BuildV2ModelData(model, allModels)["groupNames"];

    nlohmann::json transform;
    transform["position"] = {
        {"x", position.x},
        {"y", position.y},
        {"z", position.z}
    };
    transform["rotationDeg"] = {
        {"x", rotation.x},
        {"y", rotation.y},
        {"z", rotation.z}
    };
    transform["scale"] = {
        {"x", scale.x},
        {"y", scale.y},
        {"z", scale.z}
    };
    data["transform"] = transform;

    data["dimensions"] = {
        {"width", location.GetMWidth()},
        {"height", location.GetMHeight()},
        {"depth", location.GetMDepth()}
    };

    data["attributes"] = nlohmann::json::parse(model->GetAttributesAsJSON(), nullptr, false);
    if (data["attributes"].is_discarded()) {
        data["attributes"] = nlohmann::json::object();
    }

    return data;
}

static nlohmann::json BuildCameraData(PreviewCamera* camera, bool isDefault, const std::string& type) {
    if (camera == nullptr) {
        return nlohmann::json::object();
    }

    nlohmann::json data;
    data["name"] = camera->GetName();
    data["type"] = type;
    data["isDefault"] = isDefault;
    data["position"] = {
        {"x", camera->GetPosX()},
        {"y", camera->GetPosY()},
        {"z", camera->GetPosZ()}
    };
    data["anglesDeg"] = {
        {"x", camera->GetAngleX()},
        {"y", camera->GetAngleY()},
        {"z", camera->GetAngleZ()}
    };
    data["distance"] = camera->GetDistance();
    data["zoom"] = camera->GetZoom();
    data["pan"] = {
        {"x", camera->GetPanX()},
        {"y", camera->GetPanY()},
        {"z", camera->GetPanZ()}
    };
    return data;
}

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

    if (cmd == "layout.getModelGroupMembers") {
        std::string name = ReadParamString(params, "name");
        if (name.empty() || name == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name is required.", requestId), "", 422, true);
        }
        Model* model = allModels.GetModel(name);
        if (model == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MODEL_NOT_FOUND", "Model not found.", requestId), "", 404, true);
        }
        if (model->GetDisplayAs() != "ModelGroup") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name must refer to a ModelGroup.", requestId), "", 422, true);
        }
        nlohmann::json data;
        data["group"] = BuildV2ModelData(model, allModels);
        data["members"] = BuildV2ModelGroupMembersData(model, allModels);
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

    if (cmd == "layout.getSubmodels") {
        nlohmann::json data;
        data["submodels"] = BuildLayoutSubmodelsData(allModels);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getModelGeometry") {
        std::string name = ReadParamString(params, "name");
        if (name.empty() || name == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name is required.", requestId), "", 422, true);
        }
        Model* model = allModels.GetModel(name);
        if (model == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MODEL_NOT_FOUND", "Model not found.", requestId), "", 404, true);
        }
        nlohmann::json data;
        data["model"] = BuildModelGeometryData(model, allModels);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getModelNodes") {
        std::string name = ReadParamString(params, "name");
        if (name.empty() || name == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "name is required.", requestId), "", 422, true);
        }
        Model* model = allModels.GetModel(name);
        if (model == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MODEL_NOT_FOUND", "Model not found.", requestId), "", 404, true);
        }

        bool includeBufferCoords = ReadBool(ReadParamString(params, "includeBufferCoords", "true"));
        bool includeWorldCoords = ReadBool(ReadParamString(params, "includeWorldCoords", "true"));
        bool includeScreenCoords = ReadBool(ReadParamString(params, "includeScreenCoords", "false"));
        std::string camera = ReadParamString(params, "camera", "");

        const auto& location = model->GetModelScreenLocation();
        nlohmann::json nodes = nlohmann::json::array();
        uint32_t nodeCount = model->GetNodeCount();
        for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++) {
            NodeBaseClass* node = model->GetNode(nodeIndex);
            if (node == nullptr) {
                continue;
            }

            nlohmann::json nodeJson;
            nodeJson["nodeId"] = static_cast<int>(nodeIndex + 1);
            nodeJson["stringIndex"] = static_cast<int>(node->StringNum);

            nlohmann::json coordEntries = nlohmann::json::array();
            for (const auto& coord : node->Coords) {
                nlohmann::json coordJson;
                if (includeBufferCoords) {
                    coordJson["buffer"] = {
                        {"x", coord.bufX},
                        {"y", coord.bufY}
                    };
                }
                if (includeWorldCoords) {
                    float worldX = coord.screenX;
                    float worldY = coord.screenY;
                    float worldZ = coord.screenZ;
                    location.TranslatePoint(worldX, worldY, worldZ);
                    coordJson["world"] = {
                        {"x", worldX},
                        {"y", worldY},
                        {"z", worldZ}
                    };
                }
                if (includeScreenCoords) {
                    coordJson["screen"] = {
                        {"x", coord.screenX},
                        {"y", coord.screenY},
                        {"z", coord.screenZ}
                    };
                }
                coordEntries.push_back(coordJson);
            }
            nodeJson["coords"] = coordEntries;
            nodes.push_back(nodeJson);
        }

        nlohmann::json data;
        data["modelName"] = model->GetName();
        data["nodes"] = nodes;
        data["source"] = {
            {"isCustomModel", model->IsCustom()},
            {"customModelParsed", model->IsCustom()}
        };
        data["requested"] = {
            {"includeBufferCoords", includeBufferCoords},
            {"includeWorldCoords", includeWorldCoords},
            {"includeScreenCoords", includeScreenCoords}
        };
        if (!camera.empty() && camera != "null") {
            data["requested"]["camera"] = camera;
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getCameras") {
        nlohmann::json cameras = nlohmann::json::array();
        PreviewCamera* default2d = frame->viewpoint_mgr.GetDefaultCamera2D();
        PreviewCamera* default3d = frame->viewpoint_mgr.GetDefaultCamera3D();

        for (int i = 0; i < frame->viewpoint_mgr.GetNum2DCameras(); i++) {
            PreviewCamera* camera = frame->viewpoint_mgr.GetCamera2D(i);
            if (camera == nullptr) {
                continue;
            }
            bool isDefault = default2d != nullptr && camera->GetName() == default2d->GetName();
            cameras.push_back(BuildCameraData(camera, isDefault, "2D"));
        }
        for (int i = 0; i < frame->viewpoint_mgr.GetNum3DCameras(); i++) {
            PreviewCamera* camera = frame->viewpoint_mgr.GetCamera3D(i);
            if (camera == nullptr) {
                continue;
            }
            bool isDefault = default3d != nullptr && camera->GetName() == default3d->GetName();
            cameras.push_back(BuildCameraData(camera, isDefault, "3D"));
        }

        nlohmann::json data;
        data["cameras"] = cameras;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "layout.getScene") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }

        bool includeNodes = ReadBool(ReadParamString(params, "includeNodes", "false"));
        bool includeCameras = ReadBool(ReadParamString(params, "includeCameras", "true"));

        nlohmann::json models = nlohmann::json::array();
        for (auto it = (&allModels)->begin(); it != (&allModels)->end(); ++it) {
            Model* model = it->second;
            if (model == nullptr) {
                continue;
            }
            nlohmann::json modelData = BuildModelGeometryData(model, allModels);
            if (includeNodes) {
                nlohmann::json nodes = nlohmann::json::array();
                uint32_t nodeCount = model->GetNodeCount();
                for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++) {
                    NodeBaseClass* node = model->GetNode(nodeIndex);
                    if (node == nullptr) {
                        continue;
                    }
                    nlohmann::json coordEntries = nlohmann::json::array();
                    for (const auto& coord : node->Coords) {
                        float worldX = coord.screenX;
                        float worldY = coord.screenY;
                        float worldZ = coord.screenZ;
                        model->GetModelScreenLocation().TranslatePoint(worldX, worldY, worldZ);
                        coordEntries.push_back({
                            {"buffer", {{"x", coord.bufX}, {"y", coord.bufY}}},
                            {"world", {{"x", worldX}, {"y", worldY}, {"z", worldZ}}}
                        });
                    }
                    nodes.push_back({
                        {"nodeId", static_cast<int>(nodeIndex + 1)},
                        {"stringIndex", static_cast<int>(node->StringNum)},
                        {"coords", coordEntries}
                    });
                }
                modelData["nodes"] = nodes;
            }
            models.push_back(modelData);
        }

        nlohmann::json views = nlohmann::json::array();
        auto allViews = frame->GetViewsManager()->GetViews();
        for (auto* view : allViews) {
            nlohmann::json modelNames = nlohmann::json::array();
            auto viewModels = view->GetModels();
            for (const auto& modelName : viewModels) {
                modelNames.push_back(modelName);
            }
            views.push_back({
                {"name", view->GetName()},
                {"models", modelNames}
            });
        }

        nlohmann::json data;
        data["models"] = models;
        data["views"] = views;
        data["displayElements"] = BuildLayoutDisplayElementsData(sequenceElements);

        if (includeCameras) {
            nlohmann::json cameras = nlohmann::json::array();
            PreviewCamera* default2d = frame->viewpoint_mgr.GetDefaultCamera2D();
            PreviewCamera* default3d = frame->viewpoint_mgr.GetDefaultCamera3D();
            for (int i = 0; i < frame->viewpoint_mgr.GetNum2DCameras(); i++) {
                PreviewCamera* camera = frame->viewpoint_mgr.GetCamera2D(i);
                if (camera == nullptr) {
                    continue;
                }
                bool isDefault = default2d != nullptr && camera->GetName() == default2d->GetName();
                cameras.push_back(BuildCameraData(camera, isDefault, "2D"));
            }
            for (int i = 0; i < frame->viewpoint_mgr.GetNum3DCameras(); i++) {
                PreviewCamera* camera = frame->viewpoint_mgr.GetCamera3D(i);
                if (camera == nullptr) {
                    continue;
                }
                bool isDefault = default3d != nullptr && camera->GetName() == default3d->GetName();
                cameras.push_back(BuildCameraData(camera, isDefault, "3D"));
            }
            data["cameras"] = cameras;
        }

        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
