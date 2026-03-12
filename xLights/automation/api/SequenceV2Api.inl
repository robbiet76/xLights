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

    if (cmd == "sequence.getSettings") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceSettingsData(frame->CurrentSeqXmlFile), requestId), "", 200, true);
    }

    if (cmd == "sequence.getRevision") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }
        nlohmann::json data;
        data["sequencePath"] = BuildCurrentSequencePath(frame->CurrentSeqXmlFile);
        data["revisionToken"] = BuildSequenceRevisionToken(frame->CurrentSeqXmlFile, sequenceElements);
        data["lastModifiedEpochMs"] = BuildSequenceLastModifiedEpochMs(frame->CurrentSeqXmlFile);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "sequence.open") {
        std::string file = ReadParamString(params, "file");
        bool force = ReadBool(ReadParamString(params, "force", "false"));
        bool promptIssues = ReadBool(ReadParamString(params, "promptIssues", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (file.empty() || file == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "file is required.", requestId, {{"param", "file"}}), "", 422, true);
        }
        std::string seq = frame->FindSequence(file);
        if (seq.empty()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_FOUND", "Sequence not found.", requestId, {{"file", file}}), "", 404, true);
        }

        if (frame->CurrentSeqXmlFile != nullptr && !force) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "SEQUENCE_ALREADY_OPEN", "A sequence is already open.", requestId, {{"force", false}}), "", 409, true);
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
                    return sendResponse(BuildV2ErrorResponse(409, cmd, "UNSAVED_CHANGES", "Current sequence has unsaved changes.", requestId, {{"force", false}}), "", 409, true);
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
            return sendResponse(BuildV2ErrorResponse(503, cmd, "OPEN_FAILED", "Failed to open sequence.", requestId,
                                                    {{"file", seq}, {"promptIssues", promptIssues}, {"nonInteractive", !promptIssues}}), "", 503, true);
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

    if (cmd == "sequence.setSettings") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }

        std::string sequenceType = ReadParamString(params, "sequenceType");
        int durationMs = ReadParamInt(params, "durationMs", -1);
        int frameMs = ReadParamInt(params, "frameMs", -1);
        bool hasSupportsModelBlending = params.find("supportsModelBlending") != params.end();
        bool supportsModelBlending = ReadBool(ReadParamString(params, "supportsModelBlending", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        std::map<HEADER_INFO_TYPES, std::string> metadataUpdates;
        auto addMetadataIfPresent = [&](HEADER_INFO_TYPES type, const std::string& key) {
            auto it = params.find(key);
            if (it != params.end()) {
                metadataUpdates[type] = it->second;
            }
        };
        addMetadataIfPresent(HEADER_INFO_TYPES::AUTHOR, "metadataAuthor");
        addMetadataIfPresent(HEADER_INFO_TYPES::AUTHOR_EMAIL, "metadataAuthorEmail");
        addMetadataIfPresent(HEADER_INFO_TYPES::WEBSITE, "metadataWebsite");
        addMetadataIfPresent(HEADER_INFO_TYPES::SONG, "metadataSong");
        addMetadataIfPresent(HEADER_INFO_TYPES::ARTIST, "metadataArtist");
        addMetadataIfPresent(HEADER_INFO_TYPES::ALBUM, "metadataAlbum");
        addMetadataIfPresent(HEADER_INFO_TYPES::URL, "metadataMusicUrl");
        addMetadataIfPresent(HEADER_INFO_TYPES::COMMENT, "metadataComment");

        if (sequenceType.empty()) {
            sequenceType = ReadParamString(params, "sequenceType");
        }

        bool hasAny = !sequenceType.empty() || durationMs > 0 || frameMs > 0 || hasSupportsModelBlending || !metadataUpdates.empty();
        if (!hasAny) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "At least one mutable sequence setting is required.", requestId), "", 422, true);
        }

        if (dryRun) {
            nlohmann::json warnings = BuildDryRunWarnings(true);
            nlohmann::json data = BuildV2SequenceSettingsData(frame->CurrentSeqXmlFile);
            data["updated"] = true;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (!sequenceType.empty() && sequenceType != frame->CurrentSeqXmlFile->GetSequenceType().ToStdString()) {
            frame->CurrentSeqXmlFile->SetSequenceType(wxString::FromUTF8(sequenceType));
        }
        if (durationMs > 0) {
            frame->CurrentSeqXmlFile->SetSequenceDuration(static_cast<double>(durationMs) / 1000.0);
            frame->UpdateSequenceLength();
            frame->SetSequenceEnd(frame->CurrentSeqXmlFile->GetSequenceDurationMS());
        }
        if (frameMs > 0 && frameMs != frame->CurrentSeqXmlFile->GetFrameMS()) {
            frame->CurrentSeqXmlFile->SetSequenceTiming(wxString::Format("%d ms", frameMs));
            frame->SetSequenceTiming(frameMs);
            if (frame->CurrentSeqXmlFile->HasAudioMedia() && frame->CurrentSeqXmlFile->GetMedia() != nullptr) {
                frame->CurrentSeqXmlFile->GetMedia()->SetFrameInterval(frameMs);
            }
        }
        if (hasSupportsModelBlending) {
            frame->CurrentSeqXmlFile->setSupportsModelBlending(supportsModelBlending);
            frame->GetSequenceElements().SetSupportsModelBlending(supportsModelBlending);
        }
        for (const auto& entry : metadataUpdates) {
            frame->CurrentSeqXmlFile->SetHeaderInfo(entry.first, wxString::FromUTF8(entry.second));
        }

        return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2SequenceSettingsData(frame->CurrentSeqXmlFile), requestId), "", 200, true);
    }

    if (cmd == "sequence.save") {
        if (frame->CurrentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId, {{"operation", "save"}}), "", 404, true);
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
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Saving unnamed sequence requires file.", requestId,
                                                    {{"param", "file"}, {"hasSequenceName", false}}), "", 422, true);
        }

        wxFileName targetSeqFile;
        if (!file.empty()) {
            targetSeqFile.Assign(wxString::FromUTF8(file));
            targetSeqFile.SetExt("xsq");
        } else {
            targetSeqFile.Assign(frame->CurrentSeqXmlFile->GetFullPath());
        }
        wxString backupPath;
        if (targetSeqFile.FileExists()) {
            backupPath = targetSeqFile.GetFullPath() + ".automation-bak";
            wxRemoveFile(backupPath);
            wxCopyFile(targetSeqFile.GetFullPath(), backupPath, true);
        }

        wxString originalSeqPath = frame->CurrentSeqXmlFile->GetPath();
        wxString originalSeqName = frame->CurrentSeqXmlFile->GetFullName();
        bool saveAs = !file.empty();
        if (saveAs) {
            frame->CurrentSeqXmlFile->SetPath(targetSeqFile.GetPath());
            frame->CurrentSeqXmlFile->SetFullName(targetSeqFile.GetFullName());
            wxFileName fseqName(targetSeqFile.GetFullPath());
            fseqName.SetExt("fseq");
            frame->xlightsFilename = fseqName.GetFullPath();
        }

        auto oldRenderMode = frame->_renderMode;
        auto oldPromptIssues = frame->_promptBatchRenderIssues;
        frame->_renderMode = true;
        frame->_promptBatchRenderIssues = false;
        // Keep API save calls non-interactive in debug builds where wx asserts can open modal dialogs.
        ScopedAutomationAssertSuppressor suppressor(true);
        bool xmlSaveOk = frame->CurrentSeqXmlFile->Save(sequenceElements);
        frame->_promptBatchRenderIssues = oldPromptIssues;
        frame->_renderMode = oldRenderMode;

        if (!xmlSaveOk) {
            frame->CurrentSeqXmlFile->SetPath(originalSeqPath);
            frame->CurrentSeqXmlFile->SetFullName(originalSeqName);
        }

        wxULongLong savedSize = targetSeqFile.GetSize();
        bool savedOk = xmlSaveOk && targetSeqFile.FileExists() && savedSize != wxInvalidSize && savedSize.GetValue() > 0;
        if (!savedOk) {
            if (!backupPath.IsEmpty() && wxFileExists(backupPath)) {
                wxCopyFile(backupPath, targetSeqFile.GetFullPath(), true);
                wxRemoveFile(backupPath);
            }
            return sendResponse(BuildV2ErrorResponse(500, cmd, "SAVE_FAILED", "Failed to persist sequence file safely.", requestId), "", 500, true);
        }
        frame->mSavedChangeCount = sequenceElements.GetChangeCount();
        frame->mLastAutosaveCount = frame->mSavedChangeCount;
        if (!backupPath.IsEmpty() && wxFileExists(backupPath)) {
            wxRemoveFile(backupPath);
        }

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
