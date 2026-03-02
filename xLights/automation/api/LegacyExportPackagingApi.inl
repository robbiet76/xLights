namespace automation::api {

static std::optional<bool> HandleLegacyExportPackagingCommand(
    ModelManager& allModels,
    OutputModelManager& outputModelManager,
    bool& lowDefinitionRender,
    const wxString& currentDir,
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<void(const wxString&)>& exportModelsFn,
    const std::function<bool(const std::string&, const std::string&, const std::string&, bool)>& doExportModelFn,
    const std::function<std::string()>& packageSequenceFn,
    const std::function<std::string()>& packageDebugFilesFn,
    const std::function<bool(const wxString&)>& exportVideoPreviewFn,
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

    if (cmd == "exportModelsCSV") {
        auto filename = getParam("filename");
        if (filename == "" || filename == "null") {
            wxFileName f;
            f.AssignTempFileName("Models_");
            filename = f.GetFullPath();
        }

        exportModelsFn(filename);

        std::string response = wxString::Format("{\"msg\":\"Models Exported.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);
    }

    auto mapExportFormat = [](std::string format) -> std::string {
        if (format == "lsp") {
            return "LSP";
        } else if (format == "lorclipboard") {
            return "Lcb";
        } else if (format == "lorclipboards5") {
            return "LcbS5";
        } else if (format == "vixenroutine") {
            return "Vir";
        } else if (format == "hls") {
            return "HLS";
        } else if (format == "eseq") {
            return "FPP";
        } else if (format == "eseqcompressed") {
            return "FPPCompressed";
        } else if (format == "avicompressed" || format == "mp4compressed") {
            return "Com";
        } else if (format == "aviuncompressed" || format == "mp4uncompressed") {
            return "Unc";
        } else if (format == "minleon") {
            return "Min";
        } else if (format == "gif") {
            return "GIF";
        }
        return "";
    };

    if (cmd == "exportModel") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto model = getParam("model");
        if (allModels.GetModel(model) == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }

        auto filename = getParam("filename");
        auto format = mapExportFormat(getParam("format"));
        if (format.empty()) {
            return sendResponse("Unknown format.", "msg", 503, false);
        }

        if (doExportModelFn(model, filename, format, false)) {
            return sendResponse("Model exported.", "msg", 200, false);
        }
        return sendResponse("Failed to export.", "msg", 503, false);
    }

    if (cmd == "exportModelWithRender") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto ld = lowDefinitionRender;
        auto highdef = getParam("highdef");
        auto model = getParam("model");

        if (allModels.GetModel(model) == nullptr) {
            return sendResponse("Unknown model.", "msg", 503, false);
        }

        if (highdef == "true" && lowDefinitionRender) {
            lowDefinitionRender = false;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_ALLMODELS, "Automation::exportModelWithRender");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
        }

        auto filename = getParam("filename");
        auto format = mapExportFormat(getParam("format"));
        if (format.empty()) {
            return sendResponse("Unknown format.", "msg", 503, false);
        }

        if (doExportModelFn(model, filename, format, true)) {
            if (ld != lowDefinitionRender) {
                lowDefinitionRender = ld;
                outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_ALLMODELS, "Automation::exportModelWithRender");
                outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
            }
            return sendResponse("Model exported.", "msg", 200, false);
        }

        if (ld != lowDefinitionRender) {
            lowDefinitionRender = ld;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::exportModelWithRender");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::exportModelWithRender");
        }
        return sendResponse("Failed to export.", "msg", 503, false);
    }

    if (cmd == "packageSequence") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }
        auto const filename = packageSequenceFn();
        std::string response = wxString::Format("{\"msg\":\"Sequence Packaged.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);
    }

    if (cmd == "packageLogFiles") {
        auto const filename = packageDebugFilesFn();
        std::string response = wxString::Format("{\"msg\":\"Log Files Packaged.\",\"output\":\"%s\"}", JSONSafe(filename));
        return sendResponse(response, "", 200, true);
    }

    if (cmd == "exportVideoPreview") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        auto filename = getParam("filename");
        if (filename == "" || filename == "null") {
            filename = currentDir + wxFileName::GetPathSeparator() + currentSeqXmlFile->GetName() + ".mp4";
        }
        auto const worked = exportVideoPreviewFn(filename);
        if (worked) {
            std::string response = wxString::Format("{\"msg\":\"Export Video Preview.\",\"output\":\"%s\"}", JSONSafe(filename));
            return sendResponse(response, "", 200, true);
        }
        return sendResponse("Export Video Preview Failed", "msg", 503, true);
    }

    return std::nullopt;
}

} // namespace automation::api
