namespace automation::api {

static std::optional<bool> HandleSystemV2Command(
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "system.getCapabilities") {
        nlohmann::json data;
        data["apiVersions"] = { 2 };
        data["commands"] = GetV2Commands();

        data["features"] = {
            {"vampPluginsAvailable", false},
            {"remoteAudioAnalysisAvailable", true},
            {"lyricsSrtImportAvailable", true},
            {"songStructureDetectionAvailable", false}
        };
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "system.validateCommands") {
        std::string rawCommands = ReadParamString(params, "commands");
        if (rawCommands.empty() || rawCommands == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "commands is required.", requestId), "", 422, true);
        }

        auto parsedCommands = nlohmann::json::parse(rawCommands, nullptr, false);
        if (!parsedCommands.is_array() || parsedCommands.is_discarded() || parsedCommands.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "commands must be a non-empty array.", requestId), "", 422, true);
        }

        std::set<std::string> commandSet(GetV2Commands().begin(), GetV2Commands().end());
        nlohmann::json results = nlohmann::json::array();
        bool valid = true;

        for (size_t i = 0; i < parsedCommands.size(); i++) {
            const auto& command = parsedCommands[i];
            std::string errorCode;
            std::string errorMessage;

            nlohmann::json entry;
            entry["index"] = static_cast<int>(i);
            if (command.is_object() && command.contains("cmd") && command["cmd"].is_string()) {
                entry["cmd"] = command["cmd"].get<std::string>();
            }

            bool commandValid = ValidateBatchCommandShape(command, commandSet, errorCode, errorMessage);
            entry["valid"] = commandValid;
            if (!commandValid) {
                valid = false;
                entry["error"] = {
                    {"code", errorCode.empty() ? "VALIDATION_ERROR" : errorCode},
                    {"message", GetValidationErrorMessage(errorMessage)}
                };
            }
            results.push_back(entry);
        }

        nlohmann::json data;
        data["valid"] = valid;
        data["results"] = results;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
