static bool HasArrayParam(const nlohmann::json& params, const std::string& key) {
    return params.contains(key) && params[key].is_array();
}

static std::string GetValidationErrorMessage(const std::string& message) {
    return message.empty() ? "Validation failed." : message;
}

static bool ValidateTimingMarksParamShape(const nlohmann::json& params, std::string& errorMessage) {
    if (!HasArrayParam(params, "marks") || params["marks"].empty()) {
        errorMessage = "timing.insertMarks requires non-empty params.marks.";
        return false;
    }

    int previousStart = -1;
    int previousEnd = -1;
    bool hasPreviousEnd = false;
    for (size_t i = 0; i < params["marks"].size(); i++) {
        const auto& mark = params["marks"][i];
        if (!mark.is_object()) {
            errorMessage = "marks entries must be objects.";
            return false;
        }
        if (!mark.contains("startMs") || !mark["startMs"].is_number_integer()) {
            errorMessage = "marks[].startMs must be an integer.";
            return false;
        }
        int startMs = mark["startMs"].get<int>();
        if (startMs < 0) {
            errorMessage = "marks[].startMs must be >= 0.";
            return false;
        }

        bool hasEnd = mark.contains("endMs") && !mark["endMs"].is_null();
        int endMs = -1;
        if (hasEnd) {
            if (!mark["endMs"].is_number_integer()) {
                errorMessage = "marks[].endMs must be an integer when provided.";
                return false;
            }
            endMs = mark["endMs"].get<int>();
            if (endMs <= startMs) {
                errorMessage = "marks[].endMs must be > startMs.";
                return false;
            }
        }
        if (mark.contains("label") && !(mark["label"].is_string() || mark["label"].is_null())) {
            errorMessage = "marks[].label must be a string when provided.";
            return false;
        }

        if (previousStart != -1 && startMs < previousStart) {
            errorMessage = "marks must be ordered by startMs.";
            return false;
        }
        if (hasPreviousEnd && startMs < previousEnd) {
            errorMessage = "marks must not overlap.";
            return false;
        }
        previousStart = startMs;
        if (hasEnd) {
            previousEnd = endMs;
            hasPreviousEnd = true;
        } else {
            hasPreviousEnd = false;
        }
    }

    return true;
}

static bool ValidateDisplayOrderParams(const nlohmann::json& params, std::string& errorMessage) {
    if (!HasArrayParam(params, "orderedIds") || params["orderedIds"].empty()) {
        errorMessage = "sequencer.setDisplayElementOrder requires non-empty params.orderedIds.";
        return false;
    }

    std::set<std::string> seen;
    for (const auto& id : params["orderedIds"]) {
        if (!id.is_string() || id.get<std::string>().empty()) {
            errorMessage = "orderedIds must contain non-empty strings.";
            return false;
        }
        if (!seen.insert(id.get<std::string>()).second) {
            errorMessage = "orderedIds must not contain duplicates.";
            return false;
        }
    }
    return true;
}

static bool IsValidEffectSelectorValue(const nlohmann::json& value) {
    if (value.is_string()) {
        std::string s = value.get<std::string>();
        return !s.empty() && s != "null";
    }
    if (value.is_number_integer()) {
        return value.get<int>() > 0;
    }
    return false;
}

static bool ValidateEffectSelectorParams(const nlohmann::json& params, std::string& errorMessage) {
    bool hasSelector = false;

    if (params.contains("modelName")) {
        if (!params["modelName"].is_string() || params["modelName"].get<std::string>().empty()) {
            errorMessage = "modelName must be a non-empty string when provided.";
            return false;
        }
        hasSelector = true;
    }
    if (params.contains("layerIndex")) {
        if (!params["layerIndex"].is_number_integer() || params["layerIndex"].get<int>() < 0) {
            errorMessage = "layerIndex must be >= 0 when provided.";
            return false;
        }
    }
    if (params.contains("effectId")) {
        if (!IsValidEffectSelectorValue(params["effectId"])) {
            errorMessage = "effectId must be a non-empty string or positive integer.";
            return false;
        }
        hasSelector = true;
    }
    if (params.contains("effectIds")) {
        if (!params["effectIds"].is_array() || params["effectIds"].empty()) {
            errorMessage = "effectIds must be a non-empty array when provided.";
            return false;
        }
        for (const auto& id : params["effectIds"]) {
            if (!IsValidEffectSelectorValue(id)) {
                errorMessage = "effectIds entries must be non-empty strings or positive integers.";
                return false;
            }
        }
        hasSelector = true;
    }

    if (!hasSelector) {
        errorMessage = "An effect selector is required (modelName, effectId, or effectIds).";
        return false;
    }
    return true;
}

static bool ValidateBatchCommandShape(const nlohmann::json& command,
                                      const std::set<std::string>& commandSet,
                                      std::string& errorCode,
                                      std::string& errorMessage) {
    if (!command.is_object()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[] entries must be objects.";
        return false;
    }
    if (!command.contains("cmd") || !command["cmd"].is_string() || command["cmd"].get<std::string>().empty()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[].cmd must be a non-empty string.";
        return false;
    }

    std::string childCmd = command["cmd"].get<std::string>();
    if (commandSet.find(childCmd) == commandSet.end()) {
        errorCode = "UNKNOWN_COMMAND";
        errorMessage = "Unsupported command: '" + childCmd + "'.";
        return false;
    }

    const nlohmann::json params = command.contains("params") ? command["params"] : nlohmann::json::object();
    if (!params.is_object()) {
        errorCode = "BAD_REQUEST";
        errorMessage = "commands[].params must be an object.";
        return false;
    }
    if (params.contains("expectedRevision") && !(params["expectedRevision"].is_string() || params["expectedRevision"].is_null())) {
        errorCode = "VALIDATION_ERROR";
        errorMessage = "expectedRevision must be a string when provided.";
        return false;
    }

    if (command.contains("options")) {
        const auto& options = command["options"];
        if (!options.is_object()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options must be an object.";
            return false;
        }
        if (options.contains("requestId") && !options["requestId"].is_string()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options.requestId must be a string.";
            return false;
        }
        if (options.contains("dryRun") && !options["dryRun"].is_boolean() && !options["dryRun"].is_number_integer()) {
            errorCode = "BAD_REQUEST";
            errorMessage = "commands[].options.dryRun must be a boolean or integer.";
            return false;
        }
    }

    if (childCmd == "system.validateCommands") {
        errorCode = "VALIDATION_ERROR";
        errorMessage = "Nested system.validateCommands is not allowed.";
        return false;
    }
    if (childCmd == "system.executePlan") {
        errorCode = "VALIDATION_ERROR";
        errorMessage = "Nested system.executePlan is not allowed.";
        return false;
    }

    if (childCmd == "system.executePlan") {
        if (!params.contains("commands") || !params["commands"].is_array() || params["commands"].empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "system.executePlan requires params.commands.";
            return false;
        }
    } else if (childCmd == "transactions.begin") {
        // no required params
    } else if (childCmd == "transactions.commit") {
        if (!params.contains("transactionId") || !params["transactionId"].is_string() || params["transactionId"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "transactions.commit requires params.transactionId.";
            return false;
        }
        if (params.contains("expectedRevision") && !(params["expectedRevision"].is_string() || params["expectedRevision"].is_null())) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "transactions.commit expectedRevision must be string when provided.";
            return false;
        }
    } else if (childCmd == "transactions.rollback") {
        if (!params.contains("transactionId") || !params["transactionId"].is_string() || params["transactionId"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "transactions.rollback requires params.transactionId.";
            return false;
        }
    } else if (childCmd == "jobs.get" || childCmd == "jobs.cancel") {
        if (!params.contains("jobId") || !params["jobId"].is_string() || params["jobId"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = childCmd + " requires params.jobId.";
            return false;
        }
    } else if (childCmd == "sequence.open") {
        if (!params.contains("file") || !params["file"].is_string() || params["file"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "sequence.open requires params.file.";
            return false;
        }
    } else if (childCmd == "sequence.create") {
        if (!params.contains("frameMs") || !params["frameMs"].is_number_integer() || params["frameMs"].get<int>() <= 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "sequence.create requires params.frameMs > 0.";
            return false;
        }
        bool hasMediaFile = params.contains("mediaFile") && !params["mediaFile"].is_null() &&
                            params["mediaFile"].is_string() && !params["mediaFile"].get<std::string>().empty();
        if (!hasMediaFile) {
            if (!params.contains("durationMs") || !params["durationMs"].is_number_integer() || params["durationMs"].get<int>() <= 0) {
                errorCode = "VALIDATION_ERROR";
                errorMessage = "sequence.create requires params.durationMs > 0 when mediaFile is absent.";
                return false;
            }
        }
    } else if (childCmd == "layout.getModel") {
        if (!params.contains("name") || !params["name"].is_string() || params["name"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "layout.getModel requires params.name.";
            return false;
        }
    } else if (childCmd == "media.set") {
        if (!params.contains("mediaFile") || !params["mediaFile"].is_string() || params["mediaFile"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "media.set requires params.mediaFile.";
            return false;
        }
    } else if (childCmd == "timing.createTrack") {
        if (!params.contains("trackName") || !params["trackName"].is_string() || params["trackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "timing.createTrack requires params.trackName.";
            return false;
        }
    } else if (childCmd == "timing.renameTrack") {
        if (!params.contains("trackName") || !params["trackName"].is_string() ||
            !params.contains("newTrackName") || !params["newTrackName"].is_string() ||
            params["trackName"].get<std::string>().empty() || params["newTrackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "timing.renameTrack requires params.trackName and params.newTrackName.";
            return false;
        }
    } else if (childCmd == "timing.deleteTrack" || childCmd == "timing.getMarks" ||
               childCmd == "timing.insertMarks" || childCmd == "timing.replaceMarks" ||
               childCmd == "timing.deleteMarks" || childCmd == "timing.getTrackSummary") {
        if (!params.contains("trackName") || !params["trackName"].is_string() || params["trackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = childCmd + " requires params.trackName.";
            return false;
        }
        if (childCmd == "timing.insertMarks" || childCmd == "timing.replaceMarks") {
            if (!ValidateTimingMarksParamShape(params, errorMessage)) {
                errorCode = "VALIDATION_ERROR";
                return false;
            }
        }
    } else if (childCmd == "sequencer.setDisplayElementOrder") {
        if (!ValidateDisplayOrderParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
    } else if (childCmd == "effects.getDefinition") {
        if (!params.contains("effectName") || !params["effectName"].is_string() || params["effectName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.getDefinition requires params.effectName.";
            return false;
        }
    } else if (childCmd == "effects.create") {
        if (!params.contains("modelName") || !params["modelName"].is_string() || params["modelName"].get<std::string>().empty() ||
            !params.contains("layerIndex") || !params["layerIndex"].is_number_integer() || params["layerIndex"].get<int>() < 0 ||
            !params.contains("effectName") || !params["effectName"].is_string() || params["effectName"].get<std::string>().empty() ||
            !params.contains("startMs") || !params["startMs"].is_number_integer() ||
            !params.contains("endMs") || !params["endMs"].is_number_integer() ||
            params["endMs"].get<int>() <= params["startMs"].get<int>()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.create requires modelName, layerIndex>=0, effectName, and endMs>startMs.";
            return false;
        }
    } else if (childCmd == "effects.alignToTiming") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
        if (!params.contains("timingTrackName") || !params["timingTrackName"].is_string() || params["timingTrackName"].get<std::string>().empty()) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.alignToTiming requires timingTrackName.";
            return false;
        }
    } else if (childCmd == "effects.shift") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
        if (!params.contains("deltaMs") || !params["deltaMs"].is_number_integer() || params["deltaMs"].get<int>() == 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.shift requires deltaMs != 0.";
            return false;
        }
    } else if (childCmd == "effects.update" || childCmd == "effects.delete") {
        if (!ValidateEffectSelectorParams(params, errorMessage)) {
            errorCode = "VALIDATION_ERROR";
            return false;
        }
    } else if (childCmd == "effects.clone") {
        if (!params.contains("sourceModelName") || !params["sourceModelName"].is_string() || params["sourceModelName"].get<std::string>().empty() ||
            !params.contains("sourceLayerIndex") || !params["sourceLayerIndex"].is_number_integer() || params["sourceLayerIndex"].get<int>() < 0 ||
            !HasArrayParam(params, "targetModels") || params["targetModels"].empty() ||
            !params.contains("targetLayerIndex") || !params["targetLayerIndex"].is_number_integer() || params["targetLayerIndex"].get<int>() < 0) {
            errorCode = "VALIDATION_ERROR";
            errorMessage = "effects.clone requires sourceModelName/sourceLayerIndex/targetModels/targetLayerIndex.";
            return false;
        }
    }

    return true;
}
