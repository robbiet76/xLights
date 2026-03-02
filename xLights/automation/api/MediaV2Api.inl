namespace automation::api {

static std::optional<bool> HandleMediaV2Command(
    xLightsFrame* frame,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "media.get") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string mediaFile = frame->CurrentSeqXmlFile->GetMediaFile().ToStdString();
        nlohmann::json data;
        data["mediaFile"] = mediaFile.empty() ? nlohmann::json(nullptr) : nlohmann::json(mediaFile);
        data["hasMedia"] = !mediaFile.empty();
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "media.set") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string mediaFile = ReadParamString(params, "mediaFile");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        if (mediaFile.empty() || mediaFile == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile is required.", requestId), "", 422, true);
        }
        wxFileName mediaPath(wxString::FromUTF8(mediaFile));
        if (!mediaPath.FileExists() || !mediaPath.IsFileReadable()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must exist and be readable.", requestId), "", 422, true);
        }

        std::string currentMedia = frame->CurrentSeqXmlFile->GetMediaFile().ToStdString();
        bool updated = currentMedia != mediaFile;
        if (!dryRun) {
            frame->CurrentSeqXmlFile->SetMediaFile(frame->GetShowDirectory(), wxString::FromUTF8(mediaFile), true);
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["mediaFile"] = mediaFile;
        data["updated"] = updated;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "media.getMetadata") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string mediaFile = ReadParamString(params, "mediaFile");
        if (!mediaFile.empty() && mediaFile != "null" && mediaFile != frame->CurrentSeqXmlFile->GetMediaFile().ToStdString()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase.", requestId), "", 422, true);
        }
        if (!frame->CurrentSeqXmlFile->HasAudioMedia() || frame->CurrentSeqXmlFile->GetMedia() == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId), "", 404, true);
        }

        auto* media = frame->CurrentSeqXmlFile->GetMedia();
        long sampleRate = media->GetRate();
        long sampleCount = media->GetTrackSize();
        int channels = media->GetChannels();
        int durationMs = sampleRate > 0 ? static_cast<int>((sampleCount * 1000L) / sampleRate) : 0;

        nlohmann::json data;
        data["durationMs"] = durationMs;
        data["sampleRate"] = sampleRate;
        data["channels"] = channels;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
