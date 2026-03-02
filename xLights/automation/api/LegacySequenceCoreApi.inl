namespace automation::api {

static std::optional<bool> HandleLegacySequenceCoreCommand(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    bool& promptBatchRenderIssues,
    bool& renderMode,
    wxString& xlightsFilename,
    const std::string& cmd,
    const std::vector<std::string>& paths,
    const std::map<std::string, std::string>& params,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "getVersion") {
        return sendResponse(GetDisplayVersionString(), "version", 200, false);
    }

    if (cmd == "openSequence" || cmd == "getOpenSequence" || cmd == "loadSequence") {
        wxString fname = "";
        if (paths.size() > 1) {
            fname = wxURI::Unescape(paths[1]);
        }
        bool force = false;
        bool prompt = false;

        std::string method;
        std::string dataRaw;
        auto methodIt = params.find("_METHOD");
        if (methodIt != params.end()) {
            method = methodIt->second;
        }
        auto dataIt = params.find("_DATA");
        if (dataIt != params.end()) {
            dataRaw = dataIt->second;
        }
        if (method == "POST" && !dataRaw.empty()) {
            wxString data = dataRaw;
            try {
                nlohmann::json val = nlohmann::json::parse(data.ToStdString());
                fname = val["seq"].get<std::string>();
                if (val.contains("promptIssues")) {
                    auto promptIt = params.find("promptIssues");
                    if (promptIt != params.end()) {
                        prompt = ReadBool(promptIt->second);
                    }
                }
                if (val.contains("force")) {
                    auto forceIt = params.find("force");
                    if (forceIt != params.end()) {
                        force = ReadBool(forceIt->second);
                    }
                }
            } catch (const std::exception& e) {
                return sendResponse(wxString::Format("Failed to parse JSON data: %s", e.what()), "msg", 503, false);
            }
        } else {
            auto seqIt = params.find("seq");
            if (seqIt != params.end() && seqIt->second != "") {
                fname = seqIt->second;
            }
            auto promptIt = params.find("promptIssues");
            if (promptIt != params.end()) {
                prompt = ReadBool(promptIt->second);
            }
            auto forceIt = params.find("force");
            if (forceIt != params.end()) {
                force = ReadBool(forceIt->second);
            }
        }
        if (fname.empty()) {
            if (frame->CurrentSeqXmlFile != nullptr) {
                std::string response = wxString::Format("{\"seq\":\"%s\",\"fullseq\":\"%s\",\"media\":\"%s\",\"len\":%u,\"framems\":%u}",
                                                        JSONSafe(frame->CurrentSeqXmlFile->GetName()),
                                                        JSONSafe(frame->CurrentSeqXmlFile->GetFullPath()),
                                                        JSONSafe(frame->CurrentSeqXmlFile->GetMediaFile()),
                                                        frame->CurrentSeqXmlFile->GetSequenceDurationMS(),
                                                        frame->CurrentSeqXmlFile->GetFrameMS());

                return sendResponse(response, "", 200, true);
            }
            return sendResponse("Sequence not open.", "msg", 503, false);
        }

        std::string seq = frame->FindSequence(fname);
        if (seq.empty()) {
            return sendResponse("Sequence not found.", "msg", 503, false);
        }
        if (frame->CurrentSeqXmlFile != nullptr && force) {
            return sendResponse("Sequence already open.", "msg", 503, false);
        }
        auto oldPrompt = promptBatchRenderIssues;
        auto oldRenderMode = renderMode;
        if (!prompt) {
            renderMode = true;
        }
        promptBatchRenderIssues = prompt; // off by default
        frame->OpenSequence(seq, nullptr);
        promptBatchRenderIssues = oldPrompt;
        renderMode = oldRenderMode;
        std::string response = wxString::Format("{\"seq\":\"%s\",\"fullseq\":\"%s\",\"media\":\"%s\",\"len\":%u,\"framems\":%u}",
                                                JSONSafe(frame->CurrentSeqXmlFile->GetName()),
                                                JSONSafe(frame->CurrentSeqXmlFile->GetFullPath()),
                                                JSONSafe(frame->CurrentSeqXmlFile->GetMediaFile()),
                                                frame->CurrentSeqXmlFile->GetSequenceDurationMS(),
                                                frame->CurrentSeqXmlFile->GetFrameMS());

        return sendResponse(response, "", 200, true);
    }

    if (cmd == "closeSequence") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            auto quietIt = params.find("quiet");
            bool quiet = quietIt != params.end() ? ReadBool(quietIt->second) : false;
            if (!quiet) {
                return sendResponse("Sequence not open.", "msg", 503, false);
            }
            return sendResponse("Sequence closed.", "msg", 200, false);
        }

        auto forceIt = params.find("force");
        bool force = forceIt != params.end() ? ReadBool(forceIt->second) : false;
        if (frame->mSavedChangeCount != sequenceElements.GetChangeCount()) {
            if (force) {
                frame->mSavedChangeCount = sequenceElements.GetChangeCount();
            } else {
                return sendResponse("Sequence has unsaved changes.", "msg", 504, false);
            }
        }

        frame->AskCloseSequence();
        return sendResponse("Sequence closed.", "msg", 200, false);
    }

    if (cmd == "newSequence") {
        auto forceIt = params.find("force");
        bool force = forceIt != params.end() ? ReadBool(forceIt->second) : false;
        if (frame->CurrentSeqXmlFile != nullptr && !force) {
            return sendResponse("Sequence already open.", "msg", 503, false);
        }

        std::string media;
        auto mediaIt = params.find("mediaFile");
        if (mediaIt != params.end()) {
            media = mediaIt->second;
        }
        if (media == "null") {
            media = "";
        }

        int duration = 0;
        auto durationIt = params.find("durationSecs");
        if (durationIt != params.end()) {
            duration = wxAtoi(durationIt->second) * 1000;
        }

        uint32_t frameMS = 0;
        auto frameIt = params.find("frameMS");
        if (frameIt != params.end()) {
            frameMS = wxAtoi(frameIt->second);
        }

        std::string view;
        auto viewIt = params.find("view");
        if (viewIt != params.end()) {
            view = viewIt->second;
        }
        if (view == "null") {
            view = "";
        }

        frame->NewSequence(media, duration, frameMS, view);
        frame->EnableSequenceControls(true);
        return sendResponse("Sequence created.", "msg", 200, false);
    }

    if (cmd == "saveSequence") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse("No sequence open.", "msg", 503, false);
        }
        std::string seq;
        auto seqIt = params.find("seq");
        if (seqIt != params.end()) {
            seq = seqIt->second;
        }
        if ((seq == "" || seq == "null") && xlightsFilename.IsEmpty()) {
            return sendResponse("Saving unnamed sequence needs a name to be sent.", "msg", 503, false);
        }

        auto oldRenderMode = renderMode;
        auto oldPromptIssues = promptBatchRenderIssues;
        renderMode = true;
        promptBatchRenderIssues = false;
        ScopedAutomationAssertSuppressor suppressor(true);
        if (seq != "" && seq != "null") {
            frame->SaveAsSequence(seq);
        } else {
            frame->SaveSequence();
        }
        promptBatchRenderIssues = oldPromptIssues;
        renderMode = oldRenderMode;
        return sendResponse("Sequence Saved.", "msg", 200, false);
    }

    return std::nullopt;
}

} // namespace automation::api
