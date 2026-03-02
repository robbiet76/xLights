namespace automation::api {

static std::optional<bool> HandleSequenceV2Command(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "sequence.getOpen") {
        nlohmann::json data;
        if (frame->CurrentSeqXmlFile == nullptr) {
            data["isOpen"] = false;
            data["sequence"] = nullptr;
        } else {
            data["isOpen"] = true;
            data["sequence"] = BuildV2SequenceData(frame->CurrentSeqXmlFile);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "sequence.open") {
        std::string file = ReadParamString(params, "file");
        bool force = ReadBool(ReadParamString(params, "force", "false"));
        bool promptIssues = ReadBool(ReadParamString(params, "promptIssues", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (file.empty() || file == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "file is required.", requestId), "", 422, true);
        }
        std::string seq = frame->FindSequence(file);
        if (seq.empty()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_FOUND", "Sequence not found.", requestId), "", 404, true);
        }

        if (frame->CurrentSeqXmlFile != nullptr && !force) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "SEQUENCE_ALREADY_OPEN", "A sequence is already open.", requestId), "", 409, true);
        }

        if (dryRun) {
            nlohmann::json warnings = BuildDryRunWarnings(true);
            nlohmann::json data;
            data["file"] = seq;
            data["validated"] = true;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (frame->CurrentSeqXmlFile != nullptr) {
            if (frame->mSavedChangeCount != sequenceElements.GetChangeCount()) {
                if (force) {
                    frame->mSavedChangeCount = sequenceElements.GetChangeCount();
                } else {
                    return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Current sequence has unsaved changes.", requestId), "", 409, true);
                }
            }
            frame->AskCloseSequence();
        }

        auto oldPrompt = frame->_promptBatchRenderIssues;
        auto oldRenderMode = frame->_renderMode;
        if (!promptIssues) {
            frame->_renderMode = true;
        }
        frame->_promptBatchRenderIssues = promptIssues;
        frame->OpenSequence(seq, nullptr);
        frame->_promptBatchRenderIssues = oldPrompt;
        frame->_renderMode = oldRenderMode;

        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(503, cmd, "OPEN_FAILED", "Failed to open sequence.", requestId), "", 503, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceData(frame->CurrentSeqXmlFile), requestId), "", 200, true);
    }

    if (cmd == "sequence.create") {
        std::string mediaFile = ReadParamString(params, "mediaFile");
        int durationMs = ReadParamInt(params, "durationMs", 0);
        int frameMs = ReadParamInt(params, "frameMs", 0);
        std::string view = ReadParamString(params, "view");
        bool force = ReadBool(ReadParamString(params, "force", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (frameMs <= 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "frameMs must be > 0.", requestId), "", 422, true);
        }
        if ((mediaFile.empty() || mediaFile == "null") && durationMs <= 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "durationMs must be > 0 when mediaFile is not provided.", requestId), "", 422, true);
        }
        if (!mediaFile.empty() && mediaFile != "null") {
            wxFileName mediaPath(wxString::FromUTF8(mediaFile));
            if (!mediaPath.FileExists() || !mediaPath.IsFileReadable()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must exist and be readable.", requestId), "", 422, true);
            }
        }
        if (frame->CurrentSeqXmlFile != nullptr && !force) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "SEQUENCE_ALREADY_OPEN", "A sequence is already open.", requestId), "", 409, true);
        }

        if (dryRun) {
            nlohmann::json warnings = BuildDryRunWarnings(true);
            nlohmann::json data;
            data["validated"] = true;
            data["frameMs"] = frameMs;
            if (!mediaFile.empty() && mediaFile != "null") {
                data["mediaFile"] = mediaFile;
            } else {
                data["durationMs"] = durationMs;
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (frame->CurrentSeqXmlFile != nullptr) {
            if (frame->mSavedChangeCount != sequenceElements.GetChangeCount()) {
                if (force) {
                    frame->mSavedChangeCount = sequenceElements.GetChangeCount();
                } else {
                    return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Current sequence has unsaved changes.", requestId), "", 409, true);
                }
            }
            frame->AskCloseSequence();
        }

        if (mediaFile == "null") {
            mediaFile.clear();
        }
        if (view == "null") {
            view.clear();
        }
        int durationSecs = durationMs > 0 ? durationMs / 1000 : 0;
        frame->NewSequence(mediaFile, durationSecs * 1000, frameMs, view);
        frame->EnableSequenceControls(true);

        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(503, cmd, "CREATE_FAILED", "Failed to create sequence.", requestId), "", 503, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceData(frame->CurrentSeqXmlFile), requestId), "", 200, true);
    }

    if (cmd == "sequence.save") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }
        std::string file = ReadParamString(params, "file");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (file == "null") {
            file.clear();
        }

        if (dryRun) {
            nlohmann::json warnings = BuildDryRunWarnings(true);
            nlohmann::json data;
            data["saved"] = true;
            if (!file.empty()) {
                data["file"] = file;
            } else if (!frame->xlightsFilename.IsEmpty()) {
                data["file"] = frame->xlightsFilename.ToStdString();
            } else {
                data["file"] = frame->CurrentSeqXmlFile->GetFullPath().ToStdString();
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (file.empty() && frame->xlightsFilename.IsEmpty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Saving unnamed sequence requires file.", requestId), "", 422, true);
        }

        auto oldRenderMode = frame->_renderMode;
        auto oldPromptIssues = frame->_promptBatchRenderIssues;
        frame->_renderMode = true;
        frame->_promptBatchRenderIssues = false;
        // Keep API save calls non-interactive in debug builds where wx asserts can open modal dialogs.
        ScopedAutomationAssertSuppressor suppressor(true);
        if (!file.empty()) {
            frame->SaveAsSequence(file);
        } else {
            frame->SaveSequence();
        }
        frame->_promptBatchRenderIssues = oldPromptIssues;
        frame->_renderMode = oldRenderMode;

        nlohmann::json data;
        data["saved"] = true;
        if (!file.empty()) {
            data["file"] = file;
        } else if (!frame->xlightsFilename.IsEmpty()) {
            data["file"] = frame->xlightsFilename.ToStdString();
        } else {
            data["file"] = frame->CurrentSeqXmlFile->GetFullPath().ToStdString();
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "sequence.close") {
        bool force = ReadBool(ReadParamString(params, "force", "false"));
        bool quiet = ReadBool(ReadParamString(params, "quiet", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (frame->CurrentSeqXmlFile == nullptr) {
            if (quiet) {
                nlohmann::json data;
                data["closed"] = true;
                return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
            }
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }

        if (frame->mSavedChangeCount != sequenceElements.GetChangeCount() && !force) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Sequence has unsaved changes.", requestId), "", 409, true);
        }

        if (dryRun) {
            nlohmann::json warnings = BuildDryRunWarnings(true);
            nlohmann::json data;
            data["closed"] = true;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (frame->mSavedChangeCount != sequenceElements.GetChangeCount() && force) {
            frame->mSavedChangeCount = sequenceElements.GetChangeCount();
        }
        frame->AskCloseSequence();

        nlohmann::json data;
        data["closed"] = true;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
