namespace automation::api {

static std::optional<bool> HandleTimingAnalysisV2Command(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    const std::string& showFolder,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "timing.listAnalysisPlugins") {
        std::string analysisUrl = ReadParamStringOrEnv(params, "analysisUrl", "XLIGHTS_ANALYSIS_URL");
        nlohmann::json warnings = nlohmann::json::array();
        if (analysisUrl.empty()) {
            warnings.push_back({
                {"code", "REMOTE_URL_NOT_CONFIGURED"},
                {"message", "Set analysisUrl or XLIGHTS_ANALYSIS_URL to enable remote analysis."}
            });
        }

        nlohmann::json data;
        data["providers"] = nlohmann::json::array({
            {{"id", "remote"}, {"name", "Remote Analysis Service"}, {"available", !analysisUrl.empty()}}
        });
        data["profiles"] = nlohmann::json::array({ "beats_v1", "bars_v1", "energy_v1", "structure_v1" });
        if (!analysisUrl.empty()) {
            data["analysisUrl"] = analysisUrl;
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.createFromAudio") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        if (!frame->CurrentSeqXmlFile->HasAudioMedia() || frame->CurrentSeqXmlFile->GetMedia() == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId,
                                                    {{"analysisProvider", "remote"}}), "", 404, true);
        }
        std::string trackName = ReadParamString(params, "trackName");
        std::string mediaFile = ReadParamString(params, "mediaFile");
        std::string analysisProvider = ReadParamString(params, "analysisProvider", "local");
        std::string analysisProfile = ReadParamString(params, "analysisProfile", "beats_v1");
        std::string analysisUrl = ReadParamStringOrEnv(params, "analysisUrl", "XLIGHTS_ANALYSIS_URL");
        bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
        bool addToAllViews = ReadBool(ReadParamString(params, "addToAllViews", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        bool asyncRequested = ReadBool(ReadParamString(params, "async", "false"));

        if (trackName.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }
        if (!mediaFile.empty() && mediaFile != "null" && mediaFile != frame->CurrentSeqXmlFile->GetMediaFile()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase." , requestId), "", 422, true);
        }

        TimingElement* existingTrack = sequenceElements.GetTimingElement(trackName);
        if (existingTrack != nullptr && !replaceIfExists) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
        }

        if (analysisProvider != "remote") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "UNSUPPORTED_PROVIDER", "analysisProvider must be 'remote'.", requestId,
                                                    {{"analysisProvider", analysisProvider}}), "", 422, true);
        }
        if (analysisUrl.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "analysisUrl (or XLIGHTS_ANALYSIS_URL) is required for analysisProvider=remote.", requestId,
                                                    {{"analysisProvider", "remote"}}), "", 422, true);
        }

        nlohmann::json remoteRequest;
        remoteRequest["apiVersion"] = 2;
        remoteRequest["cmd"] = "timing.createFromAudio";
        remoteRequest["params"] = {
            {"trackName", trackName},
            {"mediaFile", frame->CurrentSeqXmlFile->GetMediaFile()},
            {"showFolder", showFolder},
            {"analysisProfile", analysisProfile}
        };
        remoteRequest["options"] = {
            {"dryRun", dryRun},
            {"requestId", requestId},
            {"replaceIfExists", replaceIfExists},
            {"addToAllViews", addToAllViews}
        };

        int remoteStatus = 0;
        std::string remoteText = Curl::HTTPSPost(analysisUrl,
                                                 wxString::FromUTF8(remoteRequest.dump()),
                                                 "",
                                                 "",
                                                 "JSON",
                                                 300,
                                                 {},
                                                 &remoteStatus);
        if (remoteText.empty()) {
            return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis returned an empty response.", requestId,
                                                    {{"analysisUrl", analysisUrl}, {"httpStatus", remoteStatus}}), "", 502, true);
        }

        nlohmann::json remoteJson;
        try {
            remoteJson = nlohmann::json::parse(remoteText);
        } catch (const std::exception&) {
            return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis returned invalid JSON.", requestId,
                                                    {{"analysisUrl", analysisUrl}, {"httpStatus", remoteStatus}}), "", 502, true);
        }

        if (remoteJson.contains("error")) {
            std::string message = "Remote analysis error.";
            if (remoteJson["error"].is_object() && remoteJson["error"].contains("message")) {
                message = remoteJson["error"]["message"].get<std::string>();
            }
            int status = 502;
            if (remoteJson.contains("res") && remoteJson["res"].is_number_integer()) {
                status = remoteJson["res"].get<int>();
            } else if (remoteStatus >= 400) {
                status = remoteStatus;
            }
            return sendResponse(BuildV2ErrorResponse(status, cmd, "REMOTE_ANALYSIS_FAILED", message, requestId,
                                                    {{"analysisUrl", analysisUrl}, {"httpStatus", remoteStatus}}), "", status, true);
        }

        std::vector<int> starts;
        std::vector<int> ends;
        std::vector<std::string> labels;
        if (!ParseRemoteSections(remoteJson, starts, ends, labels)) {
            return sendResponse(BuildV2ErrorResponse(502, cmd, "REMOTE_ANALYSIS_FAILED", "Remote analysis did not return timing sections.", requestId,
                                                    {{"analysisUrl", analysisUrl}, {"httpStatus", remoteStatus}}), "", 502, true);
        }

        if (!dryRun) {
            if (existingTrack != nullptr) {
                sequenceElements.DeleteElement(trackName);
            }
            frame->CurrentSeqXmlFile->AddNewTimingSection(trackName, frame, starts, ends, labels);
        }
        if (addToAllViews && !dryRun) {
            sequenceElements.AddTimingToAllViews(trackName);
        }

        std::string action = existingTrack == nullptr ? "created" : "updated";
        nlohmann::json warnings = nlohmann::json::array();
        warnings.push_back({
            {"code", "REMOTE_PROVIDER"},
            {"message", "Timing marks were generated by remote analysis service."}
        });
        if (dryRun) {
            warnings.push_back({ {"code", "DRY_RUN"}, {"message", "No changes were applied."} });
        }

        nlohmann::json data;
        data["trackName"] = trackName;
        data["action"] = action;
        data["provider"] = "remote";
        data["analysisProfile"] = analysisProfile;
        data["markCount"] = static_cast<int>(starts.size());
        data["startMs"] = starts.front();
        data["endMs"] = ends.back();
        if (asyncRequested && !dryRun) {
            std::string jobId = CreateV2Job(cmd, false);
            MarkV2JobRunning(jobId, 90);
            MarkV2JobSucceeded(jobId, data);
            nlohmann::json asyncData;
            asyncData["jobId"] = jobId;
            asyncData["status"] = "succeeded";
            asyncData["result"] = data;
            return sendResponse(BuildV2SuccessResponse(202, cmd, asyncData, requestId, warnings), "", 202, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.getTrackSummary") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }

        std::string trackName = ReadParamString(params, "trackName");
        if (trackName.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }

        std::vector<int> starts;
        int startMs = 0;
        int endMs = 0;
        int markCount = 0;
        bool haveBounds = false;
        auto layer0 = track->GetEffectLayer(0);
        if (layer0 != nullptr) {
            auto effects = layer0->GetAllEffects();
            markCount = static_cast<int>(effects.size());
            for (auto* effect : effects) {
                starts.push_back(effect->GetStartTimeMS());
                if (!haveBounds || effect->GetStartTimeMS() < startMs) {
                    startMs = effect->GetStartTimeMS();
                    haveBounds = true;
                }
                if (!haveBounds || effect->GetEndTimeMS() > endMs) {
                    endMs = effect->GetEndTimeMS();
                }
            }
        }
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

        int minMs = 0;
        int maxMs = 0;
        int avgMs = 0;
        if (starts.size() > 1) {
            long long total = 0;
            for (size_t i = 1; i < starts.size(); i++) {
                int interval = starts[i] - starts[i - 1];
                if (i == 1 || interval < minMs) {
                    minMs = interval;
                }
                if (i == 1 || interval > maxMs) {
                    maxMs = interval;
                }
                total += interval;
            }
            avgMs = static_cast<int>(total / static_cast<long long>(starts.size() - 1));
        }

        int phrases = track->GetEffectLayerCount() > 0 && track->GetEffectLayer(0) != nullptr ? track->GetEffectLayer(0)->GetEffectCount() : 0;
        int words = track->GetEffectLayerCount() > 1 && track->GetEffectLayer(1) != nullptr ? track->GetEffectLayer(1)->GetEffectCount() : 0;
        int phonemes = track->GetEffectLayerCount() > 2 && track->GetEffectLayer(2) != nullptr ? track->GetEffectLayer(2)->GetEffectCount() : 0;

        nlohmann::json data;
        data["trackName"] = trackName;
        data["markCount"] = markCount;
        data["startMs"] = startMs;
        data["endMs"] = endMs;
        data["intervalStats"] = {
            {"minMs", minMs},
            {"maxMs", maxMs},
            {"avgMs", avgMs}
        };
        data["layers"] = {
            {"phrases", phrases},
            {"words", words},
            {"phonemes", phonemes}
        };
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "timing.createBarsFromBeats") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }

        std::string sourceTrackName = ReadParamString(params, "sourceTrackName");
        std::string trackName = ReadParamString(params, "trackName");
        int beatsPerBar = ReadParamInt(params, "beatsPerBar", 4);
        bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        bool asyncRequested = ReadBool(ReadParamString(params, "async", "false"));

        if (sourceTrackName.empty() || trackName.empty() || beatsPerBar <= 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "sourceTrackName, trackName, and beatsPerBar>0 are required.", requestId), "", 422, true);
        }

        TimingElement* sourceTrack = sequenceElements.GetTimingElement(sourceTrackName);
        if (sourceTrack == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Source timing track not found: '" + sourceTrackName + "'.", requestId), "", 404, true);
        }

        auto sourceLayer = sourceTrack->GetEffectLayer(0);
        if (sourceLayer == nullptr) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Source track has no timing marks.", requestId), "", 422, true);
        }

        auto sourceEffects = sourceLayer->GetAllEffects();
        std::vector<int> beatStarts;
        int sequenceEnd = frame->CurrentSeqXmlFile->GetSequenceDurationMS();
        int lastAccepted = -1;
        for (auto* effect : sourceEffects) {
            int start = effect->GetStartTimeMS();
            if (start < 0 || start > sequenceEnd) {
                continue;
            }
            if (lastAccepted >= 0 && start <= lastAccepted) {
                continue;
            }
            beatStarts.push_back(start);
            lastAccepted = start;
        }

        if (beatStarts.size() < 2) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Insufficient valid beat marks in source track.", requestId), "", 422, true);
        }

        TimingElement* existingTrack = sequenceElements.GetTimingElement(trackName);
        if (existingTrack != nullptr && !replaceIfExists) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
        }

        std::vector<int> starts;
        std::vector<int> ends;
        std::vector<std::string> labels;
        int downbeatCount = 0;
        for (size_t i = 0; i < beatStarts.size(); i += static_cast<size_t>(beatsPerBar)) {
            int start = beatStarts[i];
            int end = (i + static_cast<size_t>(beatsPerBar) < beatStarts.size()) ? beatStarts[i + static_cast<size_t>(beatsPerBar)] : sequenceEnd;
            if (end <= start) {
                continue;
            }
            starts.push_back(start);
            ends.push_back(end);
            downbeatCount++;
            labels.push_back("Downbeat " + std::to_string(downbeatCount));
        }

        if (starts.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "No valid bar ranges could be generated.", requestId), "", 422, true);
        }

        std::string action = existingTrack == nullptr ? "created" : "updated";
        if (!dryRun) {
            if (existingTrack != nullptr) {
                sequenceElements.DeleteElement(trackName);
            }
            frame->CurrentSeqXmlFile->AddNewTimingSection(trackName, frame, starts, ends, labels);
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["action"] = action;
        data["sourceTrackName"] = sourceTrackName;
        data["beatsPerBar"] = beatsPerBar;
        data["barCount"] = static_cast<int>(starts.size());
        data["downbeatCount"] = downbeatCount;
        data["startMs"] = starts.front();
        data["endMs"] = ends.back();
        if (asyncRequested && !dryRun) {
            std::string jobId = CreateV2Job(cmd, false);
            MarkV2JobRunning(jobId, 90);
            MarkV2JobSucceeded(jobId, data);
            nlohmann::json asyncData;
            asyncData["jobId"] = jobId;
            asyncData["status"] = "succeeded";
            asyncData["result"] = data;
            return sendResponse(BuildV2SuccessResponse(202, cmd, asyncData, requestId, warnings), "", 202, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.createEnergySections") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        if (!frame->CurrentSeqXmlFile->HasAudioMedia() || frame->CurrentSeqXmlFile->GetMedia() == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "MEDIA_NOT_AVAILABLE", "Sequence media is not available.", requestId), "", 404, true);
        }

        std::string trackName = ReadParamString(params, "trackName");
        std::string mediaFile = ReadParamString(params, "mediaFile");
        bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
        int smoothingMs = ReadParamInt(params, "smoothingMs", 0);
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        bool asyncRequested = ReadBool(ReadParamString(params, "async", "false"));
        std::vector<std::string> levels = ReadParamArray(params, "levels");
        if (levels.empty()) {
            levels = { "low", "medium", "high" };
        }

        if (trackName.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }
        if (!mediaFile.empty() && mediaFile != "null" && mediaFile != frame->CurrentSeqXmlFile->GetMediaFile()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "mediaFile must match the sequence's loaded media in this phase." , requestId), "", 422, true);
        }
        if (smoothingMs < 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "smoothingMs must be >= 0.", requestId), "", 422, true);
        }

        std::vector<std::string> uniqueLevels;
        for (const auto& level : levels) {
            if (level.empty()) {
                continue;
            }
            if (std::find(uniqueLevels.begin(), uniqueLevels.end(), level) == uniqueLevels.end()) {
                uniqueLevels.push_back(level);
            }
        }
        if (uniqueLevels.size() < 2) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "levels must contain at least two distinct labels.", requestId), "", 422, true);
        }

        TimingElement* existingTrack = sequenceElements.GetTimingElement(trackName);
        if (existingTrack != nullptr && !replaceIfExists) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
        }

        int duration = frame->CurrentSeqXmlFile->GetSequenceDurationMS();
        if (duration <= 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Sequence duration must be greater than 0.", requestId), "", 422, true);
        }

        auto* media = frame->CurrentSeqXmlFile->GetMedia();
        long sampleRate = media->GetRate();
        long sampleCount = media->GetTrackSize();
        if (sampleRate <= 0 || sampleCount <= 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to analyze audio for energy sections.", requestId), "", 422, true);
        }

        int windowMs = smoothingMs > 0 ? smoothingMs : 500;
        if (windowMs < 50) {
            windowMs = 50;
        }
        long windowSamples = (sampleRate * windowMs) / 1000;
        if (windowSamples < 1) {
            windowSamples = 1;
        }
        long hopSamples = windowSamples / 2;
        if (hopSamples < 1) {
            hopSamples = 1;
        }

        std::vector<double> energies;
        std::vector<int> frameStartsMs;
        std::vector<int> frameEndsMs;
        for (long startSample = 0; startSample < sampleCount; startSample += hopSamples) {
            long endSample = std::min(sampleCount, startSample + windowSamples);
            if (endSample <= startSample) {
                continue;
            }

            double sumSq = 0.0;
            for (long i = startSample; i < endSample; i++) {
                double l = media->GetFilteredLeftData(i);
                double sample = l;
                if (media->GetChannels() > 1) {
                    double r = media->GetFilteredRightData(i);
                    sample = 0.5 * (l + r);
                }
                sumSq += sample * sample;
            }

            double n = static_cast<double>(endSample - startSample);
            energies.push_back(std::sqrt(sumSq / n));
            frameStartsMs.push_back(static_cast<int>((startSample * 1000) / sampleRate));
            frameEndsMs.push_back(static_cast<int>((endSample * 1000) / sampleRate));
        }

        if (energies.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to derive energy frames from audio.", requestId), "", 422, true);
        }

        double minEnergy = energies[0];
        double maxEnergy = energies[0];
        for (double e : energies) {
            if (e < minEnergy) {
                minEnergy = e;
            }
            if (e > maxEnergy) {
                maxEnergy = e;
            }
        }

        std::vector<int> frameBins;
        std::vector<double> frameConfidence;
        int bins = static_cast<int>(uniqueLevels.size());
        for (double e : energies) {
            double norm = (maxEnergy > minEnergy) ? ((e - minEnergy) / (maxEnergy - minEnergy)) : 0.5;
            int idx = static_cast<int>(norm * bins);
            if (idx >= bins) {
                idx = bins - 1;
            }
            if (idx < 0) {
                idx = 0;
            }
            frameBins.push_back(idx);

            double center = (static_cast<double>(idx) + 0.5) / static_cast<double>(bins);
            double conf = 1.0 - std::min(1.0, std::abs(norm - center) * 2.0);
            frameConfidence.push_back(conf);
        }

        std::vector<int> starts;
        std::vector<int> ends;
        std::vector<std::string> labels;
        std::vector<double> confidences;
        size_t segmentStart = 0;
        while (segmentStart < frameBins.size()) {
            int currentBin = frameBins[segmentStart];
            size_t segmentEnd = segmentStart + 1;
            double confTotal = frameConfidence[segmentStart];
            while (segmentEnd < frameBins.size() && frameBins[segmentEnd] == currentBin) {
                confTotal += frameConfidence[segmentEnd];
                segmentEnd++;
            }

            int sectionStart = frameStartsMs[segmentStart];
            int sectionEnd = frameEndsMs[segmentEnd - 1];
            if (sectionEnd > sectionStart) {
                starts.push_back(sectionStart);
                ends.push_back(sectionEnd);
                labels.push_back(uniqueLevels[currentBin]);
                confidences.push_back(confTotal / static_cast<double>(segmentEnd - segmentStart));
            }
            segmentStart = segmentEnd;
        }

        if (starts.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Unable to derive valid energy sections.", requestId), "", 422, true);
        }

        // Normalize to full-song contiguous coverage for deterministic machine consumers.
        starts.front() = 0;
        for (size_t i = 1; i < starts.size(); i++) {
            starts[i] = ends[i - 1];
        }
        ends.back() = duration;

        std::string action = existingTrack == nullptr ? "created" : "updated";
        if (!dryRun) {
            if (existingTrack != nullptr) {
                sequenceElements.DeleteElement(trackName);
            }
            frame->CurrentSeqXmlFile->AddNewTimingSection(trackName, frame, starts, ends, labels);
        }

        nlohmann::json sections = nlohmann::json::array();
        for (size_t i = 0; i < starts.size(); i++) {
            double confidence = std::max(0.0, std::min(1.0, confidences[i]));
            sections.push_back({
                {"label", labels[i]},
                {"startMs", starts[i]},
                {"endMs", ends[i]},
                {"confidence", confidence}
            });
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["action"] = action;
        data["sectionCount"] = static_cast<int>(sections.size());
        data["sections"] = sections;
        int coverageMs = 0;
        for (size_t i = 0; i < starts.size(); i++) {
            coverageMs += std::max(0, ends[i] - starts[i]);
        }
        data["coverageMs"] = coverageMs;
        if (asyncRequested && !dryRun) {
            std::string jobId = CreateV2Job(cmd, false);
            MarkV2JobRunning(jobId, 90);
            MarkV2JobSucceeded(jobId, data);
            nlohmann::json asyncData;
            asyncData["jobId"] = jobId;
            asyncData["status"] = "succeeded";
            asyncData["result"] = data;
            return sendResponse(BuildV2SuccessResponse(202, cmd, asyncData, requestId, warnings), "", 202, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
