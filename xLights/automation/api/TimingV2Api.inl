namespace automation::api {

static std::optional<bool> HandleTimingV2Command(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::function<std::optional<bool>(int& startMs, int& endMs, int defaultEndMs)>& normalizeRangeOrError,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "timing.getTracks") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        bool includeCounts = ReadBool(ReadParamString(params, "includeCounts", "false"));

        nlohmann::json tracks = nlohmann::json::array();
        int trackCount = sequenceElements.GetNumberOfTimingElements();
        for (int i = 0; i < trackCount; i++) {
            TimingElement* track = sequenceElements.GetTimingElement(i);
            if (track == nullptr) {
                continue;
            }
            nlohmann::json entry;
            entry["name"] = track->GetName();
            entry["type"] = track->IsFixedTiming() ? "fixed" : "variable";
            if (includeCounts) {
                int markCount = 0;
                auto* layer0 = track->GetEffectLayer(0);
                if (layer0 != nullptr) {
                    markCount = layer0->GetEffectCount();
                }
                entry["markCount"] = markCount;
            }
            tracks.push_back(entry);
        }

        nlohmann::json data;
        data["tracks"] = tracks;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "timing.createTrack") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        std::string trackType = ReadParamString(params, "trackType", "variable");
        bool replaceIfExists = ReadBool(ReadParamString(params, "replaceIfExists", "false"));
        bool addToAllViews = ReadBool(ReadParamString(params, "addToAllViews", "false"));
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }
        if (trackType.empty() || trackType == "null") {
            trackType = "variable";
        }

        TimingElement* existingTrack = sequenceElements.GetTimingElement(trackName);
        if (existingTrack != nullptr && !replaceIfExists) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + trackName + "'.", requestId), "", 409, true);
        }

        std::string action = existingTrack == nullptr ? "created" : "updated";
        if (!dryRun) {
            if (existingTrack != nullptr) {
                sequenceElements.DeleteElement(trackName);
            }
            std::string subType = trackType == "variable" ? "" : trackType;
            frame->CurrentSeqXmlFile->AddNewTimingSection(trackName, frame, subType);
            if (addToAllViews) {
                sequenceElements.AddTimingToAllViews(trackName);
            }
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["action"] = action;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.renameTrack") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        std::string newTrackName = ReadParamString(params, "newTrackName");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (trackName.empty() || trackName == "null" || newTrackName.empty() || newTrackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName and newTrackName are required.", requestId), "", 422, true);
        }

        TimingElement* sourceTrack = sequenceElements.GetTimingElement(trackName);
        if (sourceTrack == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }
        TimingElement* destTrack = sequenceElements.GetTimingElement(newTrackName);
        if (destTrack != nullptr && newTrackName != trackName) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRACK_ALREADY_EXISTS", "Timing track already exists: '" + newTrackName + "'.", requestId), "", 409, true);
        }

        if (!dryRun && newTrackName != trackName) {
            sequenceElements.RenameTimingTrack(trackName, newTrackName);
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["newTrackName"] = newTrackName;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.deleteTrack") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }

        if (!dryRun) {
            sequenceElements.DeleteElement(trackName);
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["deleted"] = true;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.getMarks") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }

        int startMs = ReadParamInt(params, "startMs", -1);
        int endMs = ReadParamInt(params, "endMs", -1);
        if (auto response = normalizeRangeOrError(startMs, endMs, frame->CurrentSeqXmlFile->GetSequenceDurationMS())) {
            return *response;
        }

        nlohmann::json marks = nlohmann::json::array();
        auto* layer0 = track->GetEffectLayer(0);
        if (layer0 != nullptr) {
            auto effects = layer0->GetAllEffects();
            for (auto* effect : effects) {
                if (effect->GetEndTimeMS() <= startMs || effect->GetStartTimeMS() >= endMs) {
                    continue;
                }
                marks.push_back({
                    {"startMs", effect->GetStartTimeMS()},
                    {"endMs", effect->GetEndTimeMS()},
                    {"label", effect->GetEffectName()}
                });
            }
        }

        nlohmann::json data;
        data["trackName"] = trackName;
        data["marks"] = marks;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "timing.insertMarks") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }

        std::vector<TimingMarkPayload> marks;
        std::string marksError;
        if (!ParseTimingMarkArray(params, "marks", marks, marksError)) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", marksError, requestId), "", 422, true);
        }
        NormalizeTimingMarks(marks, frame->CurrentSeqXmlFile->GetSequenceDurationMS(), frame->CurrentSeqXmlFile->GetFrameMS());
        if (!ValidateOrderedNonOverlapping(marks, marksError)) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", marksError, requestId), "", 422, true);
        }
        int sequenceEndMs = frame->CurrentSeqXmlFile->GetSequenceDurationMS();
        for (const auto& mark : marks) {
            if (mark.startMs >= sequenceEndMs || mark.endMs > sequenceEndMs) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "marks must be within sequence duration.", requestId), "", 422, true);
            }
        }

        auto* layer0 = track->GetEffectLayer(0);
        if (layer0 == nullptr) {
            return sendResponse(BuildV2ErrorResponse(500, cmd, "INTERNAL_ERROR", "Timing track has no primary layer.", requestId), "", 500, true);
        }

        auto existing = layer0->GetAllEffects();
        for (const auto& mark : marks) {
            for (auto* effect : existing) {
                if (mark.startMs < effect->GetEndTimeMS() && mark.endMs > effect->GetStartTimeMS()) {
                    return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Inserted marks overlap existing marks.", requestId), "", 422, true);
                }
            }
        }

        if (!dryRun) {
            for (const auto& mark : marks) {
                layer0->AddEffect(0, mark.label, "", "", mark.startMs, mark.endMs, EFFECT_NOT_SELECTED, false);
            }
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["insertedCount"] = static_cast<int>(marks.size());
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.replaceMarks") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }

        std::vector<TimingMarkPayload> marks;
        std::string marksError;
        if (!ParseTimingMarkArray(params, "marks", marks, marksError)) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", marksError, requestId), "", 422, true);
        }
        NormalizeTimingMarks(marks, frame->CurrentSeqXmlFile->GetSequenceDurationMS(), frame->CurrentSeqXmlFile->GetFrameMS());
        if (!ValidateOrderedNonOverlapping(marks, marksError)) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", marksError, requestId), "", 422, true);
        }
        int sequenceEndMs = frame->CurrentSeqXmlFile->GetSequenceDurationMS();
        for (const auto& mark : marks) {
            if (mark.startMs >= sequenceEndMs || mark.endMs > sequenceEndMs) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "marks must be within sequence duration.", requestId), "", 422, true);
            }
        }

        auto* layer0 = track->GetEffectLayer(0);
        if (layer0 == nullptr) {
            return sendResponse(BuildV2ErrorResponse(500, cmd, "INTERNAL_ERROR", "Timing track has no primary layer.", requestId), "", 500, true);
        }

        if (!dryRun) {
            while (layer0->GetEffectCount() > 0) {
                layer0->DeleteEffectByIndex(layer0->GetEffectCount() - 1);
            }
            for (const auto& mark : marks) {
                layer0->AddEffect(0, mark.label, "", "", mark.startMs, mark.endMs, EFFECT_NOT_SELECTED, false);
            }
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["replacedCount"] = static_cast<int>(marks.size());
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "timing.deleteMarks") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string trackName = ReadParamString(params, "trackName");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        if (trackName.empty() || trackName == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "trackName is required.", requestId), "", 422, true);
        }

        TimingElement* track = sequenceElements.GetTimingElement(trackName);
        if (track == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + trackName + "'.", requestId), "", 404, true);
        }
        auto* layer0 = track->GetEffectLayer(0);
        if (layer0 == nullptr) {
            return sendResponse(BuildV2ErrorResponse(500, cmd, "INTERNAL_ERROR", "Timing track has no primary layer.", requestId), "", 500, true);
        }

        std::vector<std::string> markIndexesStr = ReadParamArray(params, "markIndexes");
        int startMs = ReadParamInt(params, "startMs", -1);
        int endMs = ReadParamInt(params, "endMs", -1);
        bool hasIndexFilter = !markIndexesStr.empty();
        bool hasRangeFilter = (startMs >= 0 || endMs >= 0);
        if (!hasIndexFilter && !hasRangeFilter) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Provide markIndexes[] or startMs/endMs filter.", requestId), "", 422, true);
        }
        if (auto response = normalizeRangeOrError(startMs, endMs, frame->CurrentSeqXmlFile->GetSequenceDurationMS())) {
            return *response;
        }

        std::set<int> indexTargets;
        auto effects = layer0->GetAllEffects();
        int idx = 0;
        for (auto* effect : effects) {
            bool match = false;
            if (hasRangeFilter && !(effect->GetEndTimeMS() <= startMs || effect->GetStartTimeMS() >= endMs)) {
                match = true;
            }
            if (hasIndexFilter) {
                for (const auto& s : markIndexesStr) {
                    if (wxAtoi(s) == idx) {
                        match = true;
                        break;
                    }
                }
            }
            if (match) {
                indexTargets.insert(idx);
            }
            idx++;
        }

        int deletedCount = static_cast<int>(indexTargets.size());
        if (!dryRun) {
            std::vector<int> ordered(indexTargets.begin(), indexTargets.end());
            std::sort(ordered.begin(), ordered.end(), std::greater<int>());
            for (int removeIndex : ordered) {
                layer0->DeleteEffectByIndex(removeIndex);
            }
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["trackName"] = trackName;
        data["deletedCount"] = deletedCount;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
