namespace automation::api {

static std::optional<bool> HandleEffectsV2Command(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::function<std::optional<bool>(int& startMs, int& endMs, int defaultEndMs)>& normalizeRangeOrError,
    const std::function<void(const std::string& modelName,
                             int layerIndex,
                             int startMs,
                             int endMs,
                             const std::set<std::string>& effectHandlesFilter,
                             std::vector<EffectRef>& out)>& collectEffects,
    const std::function<void()>& refreshEffectGrid,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "effects.list") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string modelName = ReadParamString(params, "modelName");
        int layerIndex = ReadParamInt(params, "layerIndex", -1);
        int startMs = ReadParamInt(params, "startMs", 0);
        int endMs = ReadParamInt(params, "endMs", frame->CurrentSeqXmlFile->GetSequenceDurationMS());
        if (auto response = normalizeRangeOrError(startMs, endMs, frame->CurrentSeqXmlFile->GetSequenceDurationMS())) {
            return *response;
        }

        std::vector<EffectRef> refs;
        collectEffects(modelName, layerIndex, startMs, endMs, {}, refs);

        nlohmann::json effects = nlohmann::json::array();
        for (const auto& ref : refs) {
            effects.push_back({
                {"id", MakeEffectHandle(ref)},
                {"effectId", ref.effect->GetID()},
                {"modelName", ref.modelName},
                {"layerIndex", ref.layerIndex},
                {"effectName", ref.effect->GetEffectName()},
                {"startMs", ref.effect->GetStartTimeMS()},
                {"endMs", ref.effect->GetEndTimeMS()},
                {"settings", ParseJsonObjectOrEmpty(ref.effect->GetSettingsAsJSON())}
            });
        }
        nlohmann::json data;
        data["effects"] = effects;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "effects.create") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string modelName = ReadParamString(params, "modelName");
        int layerIndex = ReadParamInt(params, "layerIndex", -1);
        std::string effectName = ReadParamString(params, "effectName");
        int startMs = ReadParamInt(params, "startMs", -1);
        int endMs = ReadParamInt(params, "endMs", -1);
        std::string settingsJson = ReadParamString(params, "settings");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (modelName.empty() || modelName == "null" || layerIndex < 0 || effectName.empty() || effectName == "null" || startMs < 0 || endMs <= startMs) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "modelName, layerIndex>=0, effectName, and endMs>startMs are required.", requestId), "", 422, true);
        }
        Element* element = sequenceElements.GetElement(modelName);
        if (element == nullptr || element->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "VALIDATION_ERROR", "modelName must reference a non-timing element.", requestId), "", 404, true);
        }
        if (endMs > frame->CurrentSeqXmlFile->GetSequenceDurationMS()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "endMs must be within sequence duration.", requestId), "", 422, true);
        }

        std::string effectHandle = modelName + ":" + std::to_string(layerIndex) + ":0";
        if (!dryRun) {
            while (static_cast<int>(element->GetEffectLayerCount()) <= layerIndex) {
                element->AddEffectLayer();
            }
            auto* created = element->GetEffectLayer(layerIndex)->AddEffect(0, effectName, "", "", startMs, endMs, EFFECT_NOT_SELECTED, false);
            if (created == nullptr) {
                return sendResponse(BuildV2ErrorResponse(500, cmd, "INTERNAL_ERROR", "Failed to create effect.", requestId), "", 500, true);
            }
            if (!settingsJson.empty() && settingsJson != "null") {
                created->SetSettings(settingsJson, true, true);
            }
            effectHandle = modelName + ":" + std::to_string(layerIndex) + ":" + std::to_string(created->GetID());
            refreshEffectGrid();
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["effectId"] = effectHandle;
        data["created"] = true;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    if (cmd == "effects.update" || cmd == "effects.delete" || cmd == "effects.shift" || cmd == "effects.alignToTiming") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }

        std::string modelName = ReadParamString(params, "modelName");
        int layerIndex = ReadParamInt(params, "layerIndex", -1);
        int startMs = ReadParamInt(params, "startMs", 0);
        int endMs = ReadParamInt(params, "endMs", frame->CurrentSeqXmlFile->GetSequenceDurationMS());
        if (auto response = normalizeRangeOrError(startMs, endMs, frame->CurrentSeqXmlFile->GetSequenceDurationMS())) {
            return *response;
        }

        std::vector<std::string> selectorIds = ReadParamArray(params, "effectIds");
        std::string singleId = ReadParamString(params, "effectId");
        if (!singleId.empty() && singleId != "null") {
            selectorIds.push_back(singleId);
        }

        std::set<std::string> handleFilter;
        if (!selectorIds.empty()) {
            for (const auto& raw : selectorIds) {
                if (raw.find(':') != std::string::npos) {
                    handleFilter.insert(raw);
                    continue;
                }
                if (!modelName.empty() && modelName != "null" && layerIndex >= 0) {
                    handleFilter.insert(modelName + ":" + std::to_string(layerIndex) + ":" + raw);
                } else {
                    int numericId = wxAtoi(raw);
                    std::vector<EffectRef> allRefs;
                    collectEffects("", -1, 0, frame->CurrentSeqXmlFile->GetSequenceDurationMS(), {}, allRefs);
                    for (const auto& ref : allRefs) {
                        if (ref.effect->GetID() == numericId) {
                            handleFilter.insert(MakeEffectHandle(ref));
                        }
                    }
                }
            }
        }

        if (cmd == "effects.shift" || cmd == "effects.alignToTiming") {
            if (selectorIds.empty() && modelName.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Target selector is required.", requestId), "", 422, true);
            }
        }

        std::vector<EffectRef> refs;
        collectEffects(modelName, layerIndex, startMs, endMs, handleFilter, refs);
        if ((cmd == "effects.update" || cmd == "effects.alignToTiming") && refs.empty()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "EFFECT_NOT_FOUND", "No effects matched selector.", requestId), "", 404, true);
        }
        if (!selectorIds.empty() && refs.empty()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "EFFECT_NOT_FOUND", "No effects matched selector.", requestId), "", 404, true);
        }

        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        nlohmann::json warnings = BuildDryRunWarnings(dryRun);

        if (cmd == "effects.update") {
            std::string effectName = ReadParamString(params, "effectName");
            int newStart = ReadParamInt(params, "startMs", -1);
            int newEnd = ReadParamInt(params, "endMs", -1);
            std::string settingsJson = ReadParamString(params, "settings");

            int updatedCount = 0;
            for (auto& ref : refs) {
                int targetStart = newStart >= 0 ? newStart : ref.effect->GetStartTimeMS();
                int targetEnd = newEnd >= 0 ? newEnd : ref.effect->GetEndTimeMS();
                if (targetEnd <= targetStart) {
                    return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "Effect update produced invalid range.", requestId), "", 422, true);
                }
                if (!dryRun) {
                    if (!effectName.empty() && effectName != "null") {
                        ref.effect->SetEffectName(effectName);
                    }
                    if (newStart >= 0) {
                        ref.effect->SetStartTimeMS(targetStart);
                    }
                    if (newEnd >= 0) {
                        ref.effect->SetEndTimeMS(targetEnd);
                    }
                    if (!settingsJson.empty() && settingsJson != "null") {
                        ref.effect->SetSettings(settingsJson, true, true);
                    }
                }
                updatedCount++;
            }
            if (!dryRun) {
                refreshEffectGrid();
            }
            nlohmann::json data;
            data["updatedCount"] = updatedCount;
            if (!singleId.empty() && selectorIds.size() == 1 && updatedCount == 1) {
                data["effectId"] = MakeEffectHandle(refs.front());
                data["updated"] = true;
            }
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (cmd == "effects.delete") {
            std::map<EffectLayer*, std::vector<int>> byLayer;
            for (const auto& ref : refs) {
                byLayer[ref.layer].push_back(ref.effect->GetID());
            }
            int deletedCount = 0;
            if (!dryRun) {
                for (auto& kv : byLayer) {
                    auto& ids = kv.second;
                    std::sort(ids.begin(), ids.end(), std::greater<int>());
                    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
                    for (int id : ids) {
                        kv.first->DeleteEffect(id);
                        deletedCount++;
                    }
                }
                refreshEffectGrid();
            } else {
                std::set<std::string> uniqueHandles;
                for (const auto& ref : refs) {
                    uniqueHandles.insert(MakeEffectHandle(ref));
                }
                deletedCount = static_cast<int>(uniqueHandles.size());
            }

            nlohmann::json data;
            data["deletedCount"] = deletedCount;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (cmd == "effects.shift") {
            int deltaMs = ReadParamInt(params, "deltaMs", 0);
            bool clipToSequence = ReadBool(ReadParamString(params, "clipToSequence", "true"));
            if (deltaMs == 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "deltaMs must be non-zero.", requestId), "", 422, true);
            }
            int sequenceLen = frame->CurrentSeqXmlFile->GetSequenceDurationMS();
            int shiftedCount = 0;
            int clippedCount = 0;
            for (auto& ref : refs) {
                int start = ref.effect->GetStartTimeMS();
                int end = ref.effect->GetEndTimeMS();
                int len = std::max(1, end - start);
                int targetStart = start + deltaMs;
                int targetEnd = end + deltaMs;
                if (clipToSequence) {
                    int clippedStart = targetStart;
                    if (clippedStart < 0) {
                        clippedStart = 0;
                    }
                    if (clippedStart + len > sequenceLen) {
                        clippedStart = std::max(0, sequenceLen - len);
                    }
                    int clippedEnd = clippedStart + len;
                    if (clippedStart != targetStart || clippedEnd != targetEnd) {
                        clippedCount++;
                    }
                    targetStart = clippedStart;
                    targetEnd = clippedEnd;
                }
                if (!dryRun) {
                    ref.effect->SetStartTimeMS(targetStart);
                    ref.effect->SetEndTimeMS(targetEnd);
                }
                shiftedCount++;
            }
            if (!dryRun) {
                refreshEffectGrid();
            }
            nlohmann::json data;
            data["shiftedCount"] = shiftedCount;
            data["clippedCount"] = clippedCount;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }

        if (cmd == "effects.alignToTiming") {
            std::string timingTrackName = ReadParamString(params, "timingTrackName");
            std::string mode = ReadParamString(params, "mode", "nearest");
            if (timingTrackName.empty() || timingTrackName == "null") {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "timingTrackName is required.", requestId), "", 422, true);
            }
            TimingElement* timingTrack = sequenceElements.GetTimingElement(timingTrackName);
            if (timingTrack == nullptr) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "TRACK_NOT_FOUND", "Timing track not found: '" + timingTrackName + "'.", requestId), "", 404, true);
            }
            auto* timingLayer = timingTrack->GetEffectLayer(0);
            if (timingLayer == nullptr || timingLayer->GetEffectCount() == 0) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "timingTrackName has no marks.", requestId), "", 422, true);
            }
            std::vector<int> boundaries;
            for (auto* mark : timingLayer->GetAllEffects()) {
                boundaries.push_back(mark->GetStartTimeMS());
                boundaries.push_back(mark->GetEndTimeMS());
            }
            std::sort(boundaries.begin(), boundaries.end());
            boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

            auto nearestBoundary = [&](int value) {
                int best = boundaries.front();
                int bestDist = std::abs(best - value);
                for (int b : boundaries) {
                    int dist = std::abs(b - value);
                    if (dist < bestDist) {
                        best = b;
                        bestDist = dist;
                    }
                }
                return best;
            };
            auto floorBoundary = [&](int value) {
                int out = boundaries.front();
                for (int b : boundaries) {
                    if (b <= value) {
                        out = b;
                    } else {
                        break;
                    }
                }
                return out;
            };
            auto ceilBoundary = [&](int value) {
                int out = boundaries.back();
                for (int b : boundaries) {
                    if (b >= value) {
                        out = b;
                        break;
                    }
                }
                return out;
            };

            int alignedCount = 0;
            for (auto& ref : refs) {
                int start = ref.effect->GetStartTimeMS();
                int end = ref.effect->GetEndTimeMS();
                int newStart = start;
                int newEnd = end;
                if (mode == "expand") {
                    newStart = floorBoundary(start);
                    newEnd = ceilBoundary(end);
                } else if (mode == "contract") {
                    newStart = ceilBoundary(start);
                    newEnd = floorBoundary(end);
                } else {
                    newStart = nearestBoundary(start);
                    newEnd = nearestBoundary(end);
                }
                if (newEnd <= newStart) {
                    continue;
                }
                if (!dryRun) {
                    ref.effect->SetStartTimeMS(newStart);
                    ref.effect->SetEndTimeMS(newEnd);
                }
                alignedCount++;
            }
            if (!dryRun) {
                refreshEffectGrid();
            }
            nlohmann::json data;
            data["alignedCount"] = alignedCount;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
        }
    }

    if (cmd == "effects.clone") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        std::string sourceModelName = ReadParamString(params, "sourceModelName");
        int sourceLayerIndex = ReadParamInt(params, "sourceLayerIndex", -1);
        int startMs = ReadParamInt(params, "startMs", 0);
        int endMs = ReadParamInt(params, "endMs", frame->CurrentSeqXmlFile->GetSequenceDurationMS());
        std::vector<std::string> targetModels = ReadParamArray(params, "targetModels");
        int targetLayerIndex = ReadParamInt(params, "targetLayerIndex", -1);
        std::string mode = ReadParamString(params, "mode", "replace");
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));

        if (sourceModelName.empty() || sourceLayerIndex < 0 || targetModels.empty() || targetLayerIndex < 0) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "sourceModelName, sourceLayerIndex, targetModels, and targetLayerIndex are required.", requestId), "", 422, true);
        }
        if (auto response = normalizeRangeOrError(startMs, endMs, frame->CurrentSeqXmlFile->GetSequenceDurationMS())) {
            return *response;
        }

        std::vector<EffectRef> sourceRefs;
        collectEffects(sourceModelName, sourceLayerIndex, startMs, endMs, {}, sourceRefs);
        if (sourceRefs.empty()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "EFFECT_NOT_FOUND", "No source effects matched selector.", requestId), "", 404, true);
        }

        int createdCount = 0;
        int updatedCount = 0;
        for (const auto& targetModelName : targetModels) {
            Element* target = sequenceElements.GetElement(targetModelName);
            if (target == nullptr || target->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "VALIDATION_ERROR", "Invalid target model: '" + targetModelName + "'.", requestId), "", 404, true);
            }
            if (!dryRun) {
                while (static_cast<int>(target->GetEffectLayerCount()) <= targetLayerIndex) {
                    target->AddEffectLayer();
                }
                auto* targetLayer = target->GetEffectLayer(targetLayerIndex);
                if (mode == "replace") {
                    updatedCount += targetLayer->GetEffectCount();
                    while (targetLayer->GetEffectCount() > 0) {
                        targetLayer->DeleteEffectByIndex(targetLayer->GetEffectCount() - 1);
                    }
                }
                for (const auto& src : sourceRefs) {
                    auto* created = targetLayer->AddEffect(0,
                                                           src.effect->GetEffectName(),
                                                           src.effect->GetSettingsAsString(),
                                                           src.effect->GetPaletteAsString(),
                                                           src.effect->GetStartTimeMS(),
                                                           src.effect->GetEndTimeMS(),
                                                           EFFECT_NOT_SELECTED,
                                                           src.effect->GetProtected());
                    if (created != nullptr) {
                        createdCount++;
                    }
                }
            }
        }

        if (dryRun) {
            createdCount = static_cast<int>(sourceRefs.size() * targetModels.size());
            updatedCount = 0;
        } else {
            refreshEffectGrid();
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        nlohmann::json data;
        data["createdCount"] = createdCount;
        data["updatedCount"] = updatedCount;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
