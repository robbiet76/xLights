namespace automation::api {

static std::optional<bool> HandleLegacyRenderTransferCommand(
    xLightsFrame* frame,
    OutputManager& outputManager,
    OutputModelManager& outputModelManager,
    ModelManager& allModels,
    bool& lowDefinitionRender,
    bool& promptBatchRenderIssues,
    bool& renderMode,
    bool& saveLowDefinitionRender,
    bool& isRendering,
    const wxString& currentDir,
    xLightsXmlFile* currentSeqXmlFile,
    const std::function<std::string(const std::string&)>& findSequenceFn,
    const std::function<void()>& renderAllFn,
    const std::function<void(const wxArrayString&, bool)>& openRenderAndSaveSequencesFn,
    const std::function<void()>& yieldFn,
    const std::function<void()>& recalcModelsFn,
    const std::function<const ControllerCaps*(const std::string&)>& getControllerCapsFn,
    const std::function<bool(Controller*, wxString&)>& uploadInputToControllerFn,
    const std::function<bool(Controller*, wxString&)>& uploadOutputToControllerFn,
    const std::function<void(FPP*)>& uploadDisplayMapFn,
    const std::function<std::string(const std::string&)>& resolveFseqForXsqFn,
    const std::function<std::string(const std::string&)>& resolveMediaForXsqFn,
    const std::function<std::string(const std::string&)>& openAndCheckSequenceFn,
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

    if (cmd == "renderAll") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        auto ld = lowDefinitionRender;
        auto highdef = getParam("highdef");
        if (highdef == "true" && lowDefinitionRender) {
            lowDefinitionRender = false;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::renderAll");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::renderAll");
        }
        renderAllFn();
        while (isRendering) {
            yieldFn();
        }
        if (ld != lowDefinitionRender) {
            lowDefinitionRender = ld;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::renderAll");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::renderAll");
        }
        return sendResponse("Rendered.", "msg", 200, false);
    }

    if (cmd == "batchRender") {
        wxArrayString files;

        auto ld = lowDefinitionRender;
        auto highdef = getParam("highdef");
        if (highdef == "true" && lowDefinitionRender) {
            lowDefinitionRender = false;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::batchRender");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::batchRender");
        }

        auto seqs = getParam("seqs_0");
        int snum = 0;
        while (!seqs.empty()) {
            auto seq = findSequenceFn(seqs);
            if (seq.empty()) {
                return sendResponse("Sequence not found '" + seq + "'", "msg", 503, false);
            }
            files.push_back(seq);
            snum++;
            seqs = getParam("seqs_" + std::to_string(snum));
        }
        auto oldPrompt = promptBatchRenderIssues;
        promptBatchRenderIssues = ReadBool(getParam("promptIssues"));

        renderMode = true;
        saveLowDefinitionRender = lowDefinitionRender;
        openRenderAndSaveSequencesFn(files, false);

        while (renderMode) {
            yieldFn();
        }

        promptBatchRenderIssues = oldPrompt;
        if (ld != lowDefinitionRender) {
            lowDefinitionRender = ld;
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_RELOAD_MODEL_FROM_XML, "Automation::batchRender");
            outputModelManager.AddImmediateWork(OutputModelManager::WORK_MODELS_CHANGE_REQUIRING_RERENDER, "Automation::batchRender");
        }
        return sendResponse("Sequence batch rendered.", "msg", 200, false);
    }

    if (cmd == "uploadController") {
        auto ip = getParam("ip");
        Controller* controller = outputManager.GetControllerWithIP(ip);
        if (controller == nullptr) {
            return sendResponse("Controller not found '" + ip + "'", "msg", 503, false);
        }

        recalcModelsFn();

        bool res = true;
        auto caps = getControllerCapsFn(controller->GetName());
        if (caps != nullptr) {
            wxString message;
            if (caps->SupportsInputOnlyUpload()) {
                res = res && uploadInputToControllerFn(controller, message);
            }
            res = res && uploadOutputToControllerFn(controller, message);
        } else {
            res = false;
        }
        if (res) {
            return sendResponse("Uploaded to controller '" + ip + "'", "msg", 200, false);
        }
        return sendResponse("Upload to controller '" + ip + "' failed.", "msg", 503, false);
    }

    if (cmd == "uploadFPPConfig") {
        auto ip = getParam("ip");
        auto udp = getParam("udp");
        auto models = getParam("models");
        auto displayMap = getParam("displayMap");

        auto instances = FPP::GetInstances(frame, &outputManager);

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
            auto outputs = fpp->CreateUniverseFile(outputManager.GetControllers(), false, &udpRanges);
            fpp->UploadUDPOut(outputs);
            fpp->SetRestartFlag();
        } else if (udp == "proxy") {
            fpp->UploadUDPOutputsForProxy(&outputManager);
            fpp->SetRestartFlag();
        }

        if (models == "true" || models == "all") {
            auto memoryMaps = fpp->CreateModelMemoryMap(&allModels, 0, std::numeric_limits<int32_t>::max());
            fpp->UploadModels(memoryMaps);
        } else if (udp == "local") {
            auto controllers = outputManager.GetControllers(fpp->ipAddress);
            if (controllers.size() == 1) {
                auto const& memoryMaps = fpp->CreateModelMemoryMap(&allModels, controllers.front()->GetStartChannel(), controllers.front()->GetEndChannel());
                fpp->UploadModels(memoryMaps);
            }
        }

        if (displayMap == "true") {
            uploadDisplayMapFn(fpp);
        }

        fpp->Restart(true);
        return sendResponse("Uploaded to FPP '" + ip + "'.", "msg", 200, false);
    }

    if (cmd == "uploadSequence") {
        bool res = true;
        auto ip = getParam("ip");
        auto media = ReadBool(getParam("media"));
        auto format = getParam("format");
        auto xsq = findSequenceFn(getParam("seq"));

        if (xsq.empty()) {
            return sendResponse("Sequence not found.", "msg", 503, false);
        }

        auto fseq = resolveFseqForXsqFn(xsq);
        auto mediaPath = resolveMediaForXsqFn(xsq);

        if (!FileExists(fseq)) {
            return sendResponse("Unable to find sequence FSEQ file.", "msg", 503, false);
        }

        auto instances = FPP::GetInstances(frame, &outputManager);

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
            mediaPath = "";
        }

        FSEQFile* seq = FSEQFile::openFSEQFile(fseq);
        if (seq != nullptr) {
            fpp->PrepareUploadSequence(seq, fseq, mediaPath, fseqType);
            static const int FRAMES_TO_BUFFER = 50;
            std::vector<std::vector<uint8_t>> frames(FRAMES_TO_BUFFER);
            for (size_t i = 0; i < frames.size(); i++) {
                frames[i].resize(seq->getMaxChannel() + 1);
            }

            for (size_t frame = 0; frame < seq->getNumFrames(); frame++) {
                int lastBuffered = 0;
                size_t startFrame = frame;
                while (lastBuffered < FRAMES_TO_BUFFER && frame < seq->getNumFrames()) {
                    FSEQFile::FrameData* f = seq->getFrame(frame);
                    if (f != nullptr) {
                        if (!f->readFrame(&frames[lastBuffered][0], frames[lastBuffered].size())) {
                            res = false;
                        }
                        delete f;
                    }
                    lastBuffered++;
                    frame++;
                }
                frame--;
                for (int i = 0; i < lastBuffered; i++) {
                    fpp->AddFrameToUpload(startFrame + i, &frames[i][0]);
                }
            }
            fpp->FinalizeUploadSequence();

            if (fpp->fppType == FPP_TYPE::FALCONV4V5) {
                std::string proxy = "";
                auto controllers = outputManager.GetControllers(fpp->ipAddress);
                if (controllers.size() == 1) {
                    proxy = controllers.front()->GetFPPProxy();
                }
                Falcon falcon(fpp->ipAddress, proxy);

                if (falcon.IsConnected()) {
                    falcon.UploadSequence(fpp->GetTempFile(), fseq, fpp->mode == "remote" ? "" : mediaPath, nullptr);
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
    }

    if (cmd == "checkSequence") {
        auto seq = findSequenceFn(getParam("seq"));
        if (seq.empty()) {
            return sendResponse("Sequence not found.", "msg", 503, false);
        }
        auto outputFile = openAndCheckSequenceFn(seq);
        std::string response = wxString::Format("{\"msg\":\"Sequence checked.\",\"output\":\"%s\"}", JSONSafe(outputFile));
        return sendResponse(response, "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
