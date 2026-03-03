namespace automation::api {

static bool ParsePlanParamsToMap(const nlohmann::json& planParams, std::map<std::string, std::string>& outParams) {
    outParams.clear();
    for (auto [name, value] : planParams.items()) {
        if (value.is_array()) {
            for (size_t i = 0; i < value.size(); i++) {
                if (value[i].is_object()) {
                    for (auto [childName, childValue] : value[i].items()) {
                        std::string scalar;
                        if (!ReadScalarParam(childValue, scalar)) {
                            return false;
                        }
                        outParams[name + "_" + std::to_string(i) + "_" + childName] = scalar;
                    }
                    continue;
                }
                std::string scalar;
                if (!ReadScalarParam(value[i], scalar)) {
                    return false;
                }
                outParams[name + "_" + std::to_string(i)] = scalar;
            }
            continue;
        }
        if (value.is_object()) {
            outParams[name] = value.dump();
            continue;
        }
        std::string scalar;
        if (!ReadScalarParam(value, scalar)) {
            return false;
        }
        outParams[name] = scalar;
    }
    return true;
}

static bool ExecuteV2ChildCommand(const std::string& childCmd,
                                  const nlohmann::json& childParamsJson,
                                  const nlohmann::json& childOptionsJson,
                                  const std::string& requestId,
                                  const std::function<bool(std::vector<std::string>&,
                                                           std::map<std::string, std::string>&,
                                                           const std::function<bool(const std::string&,
                                                                                    const std::string&,
                                                                                    int,
                                                                                    bool)>&)>& processAutomation,
                                  std::string& responseBody,
                                  int& statusCode) {
    std::vector<std::string> childPaths;
    childPaths.push_back(childCmd);

    std::map<std::string, std::string> childParams;
    if (!ParsePlanParamsToMap(childParamsJson, childParams)) {
        statusCode = 422;
        responseBody = BuildV2ErrorResponse(422, childCmd, "VALIDATION_ERROR", "Unsupported params value type in executePlan command.");
        return false;
    }
    childParams["_API_VERSION"] = "2";
    if (!requestId.empty()) {
        childParams["_REQUEST_ID"] = requestId;
    }
    if (childOptionsJson.is_object() && childOptionsJson.contains("dryRun")) {
        if (childOptionsJson["dryRun"].is_boolean()) {
            childParams["_DRY_RUN"] = childOptionsJson["dryRun"].get<bool>() ? "true" : "false";
        } else if (childOptionsJson["dryRun"].is_number_integer()) {
            childParams["_DRY_RUN"] = childOptionsJson["dryRun"].get<int>() != 0 ? "true" : "false";
        }
    }

    bool callbackCalled = false;
    bool handled = processAutomation(
        childPaths,
        childParams,
        [&](const std::string& msg, const std::string& jsonKey, int responseCode, bool msgIsJSON) {
            callbackCalled = true;
            statusCode = responseCode;
            if (msgIsJSON) {
                responseBody = msg;
            } else {
                nlohmann::json fallback;
                fallback["res"] = responseCode;
                fallback[jsonKey.empty() ? "msg" : jsonKey] = msg;
                responseBody = fallback.dump();
            }
            return true;
        });
    if (!handled && !callbackCalled) {
        statusCode = 500;
        responseBody = BuildV2ErrorResponse(500, childCmd, "INTERNAL_ERROR", "Failed to execute plan command.");
        return false;
    }
    return statusCode >= 200 && statusCode < 300;
}

static std::optional<bool> HandleTransactionsV2Command(
    xLightsFrame* frame,
    SequenceElements& sequenceElements,
    xLightsXmlFile* currentSeqXmlFile,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(std::vector<std::string>&,
                             std::map<std::string, std::string>&,
                             const std::function<bool(const std::string&,
                                                      const std::string&,
                                                      int,
                                                      bool)>&)>& processAutomation,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "system.executePlan") {
        std::string rawCommands = ReadParamString(params, "commands");
        if (rawCommands.empty() || rawCommands == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "commands is required.", requestId), "", 422, true);
        }
        auto parsedCommands = nlohmann::json::parse(rawCommands, nullptr, false);
        if (!parsedCommands.is_array() || parsedCommands.is_discarded() || parsedCommands.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "commands must be a non-empty array.", requestId), "", 422, true);
        }

        bool atomic = ReadBool(ReadParamString(params, "atomic", "true"));
        bool planDryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        std::set<std::string> commandSet(GetV2Commands().begin(), GetV2Commands().end());
        nlohmann::json stepResults = nlohmann::json::array();
        bool hasMutatingCommands = false;
        for (size_t i = 0; i < parsedCommands.size(); i++) {
            const auto& command = parsedCommands[i];
            std::string errorCode;
            std::string errorMessage;
            if (!ValidateBatchCommandShape(command, commandSet, errorCode, errorMessage)) {
                return sendResponse(
                    BuildV2ErrorResponse(422, cmd, errorCode.empty() ? "VALIDATION_ERROR" : errorCode, GetValidationErrorMessage(errorMessage), requestId,
                                         {{"index", static_cast<int>(i)}}),
                    "",
                    422,
                    true);
            }
            std::string childCmd = command["cmd"].get<std::string>();
            if (IsV2MutatingCommand(childCmd)) {
                hasMutatingCommands = true;
            }
        }

        std::set<std::string> simulatedTracks;
        if (currentSeqXmlFile != nullptr) {
            int trackCount = sequenceElements.GetNumberOfTimingElements();
            for (int i = 0; i < trackCount; i++) {
                TimingElement* track = sequenceElements.GetTimingElement(i);
                if (track != nullptr) {
                    simulatedTracks.insert(track->GetName());
                }
            }
        }
        for (size_t i = 0; i < parsedCommands.size(); i++) {
            const auto& command = parsedCommands[i];
            std::string childCmd = command["cmd"].get<std::string>();
            nlohmann::json childParamsJson = command.contains("params") && command["params"].is_object() ? command["params"] : nlohmann::json::object();
            auto readTrackName = [&](const std::string& key) -> std::string {
                if (!childParamsJson.contains(key) || !childParamsJson[key].is_string()) {
                    return "";
                }
                return childParamsJson[key].get<std::string>();
            };
            auto ensureTrackExists = [&](const std::string& trackName, const std::string& op) -> std::optional<bool> {
                if (trackName.empty() || simulatedTracks.find(trackName) != simulatedTracks.end()) {
                    return std::nullopt;
                }
                return sendResponse(
                    BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", op + " references missing timing track '" + trackName + "'.", requestId,
                                         {{"index", static_cast<int>(i)}, {"cmd", childCmd}}),
                    "",
                    422,
                    true);
            };

            if (childCmd == "timing.createTrack") {
                std::string trackName = readTrackName("trackName");
                if (!trackName.empty()) {
                    simulatedTracks.insert(trackName);
                }
            } else if (childCmd == "timing.renameTrack") {
                std::string trackName = readTrackName("trackName");
                std::string newTrackName = readTrackName("newTrackName");
                if (auto response = ensureTrackExists(trackName, "timing.renameTrack")) {
                    return *response;
                }
                if (!trackName.empty() && !newTrackName.empty()) {
                    simulatedTracks.erase(trackName);
                    simulatedTracks.insert(newTrackName);
                }
            } else if (childCmd == "timing.deleteTrack") {
                std::string trackName = readTrackName("trackName");
                if (auto response = ensureTrackExists(trackName, "timing.deleteTrack")) {
                    return *response;
                }
                if (!trackName.empty()) {
                    simulatedTracks.erase(trackName);
                }
            } else if (childCmd == "timing.getMarks" || childCmd == "timing.insertMarks" || childCmd == "timing.replaceMarks" ||
                       childCmd == "timing.deleteMarks" || childCmd == "timing.getTrackSummary") {
                std::string trackName = readTrackName("trackName");
                if (auto response = ensureTrackExists(trackName, childCmd)) {
                    return *response;
                }
            }
        }

        auto appendStepResult = [&](int index, const std::string& childCmd, int statusCode, bool ok, const std::string& responseBody) {
            nlohmann::json step;
            step["index"] = index;
            step["cmd"] = childCmd;
            step["status"] = statusCode;
            step["ok"] = ok;
            auto parsed = nlohmann::json::parse(responseBody, nullptr, false);
            if (!parsed.is_discarded()) {
                step["response"] = parsed;
            }
            stepResults.push_back(step);
        };

        if (planDryRun || !atomic || !hasMutatingCommands) {
            int executed = 0;
            for (size_t i = 0; i < parsedCommands.size(); i++) {
                const auto& command = parsedCommands[i];
                std::string childCmd = command["cmd"].get<std::string>();
                nlohmann::json childParamsJson = command.contains("params") && command["params"].is_object() ? command["params"] : nlohmann::json::object();
                nlohmann::json childOptionsJson = command.contains("options") && command["options"].is_object() ? command["options"] : nlohmann::json::object();
                if (planDryRun) {
                    childOptionsJson["dryRun"] = true;
                }
                std::string body;
                int status = 0;
                bool stepOk = ExecuteV2ChildCommand(childCmd, childParamsJson, childOptionsJson, requestId, processAutomation, body, status);
                appendStepResult(static_cast<int>(i), childCmd, status, stepOk, body);
                if (!stepOk) {
                    nlohmann::json data;
                    data["atomic"] = atomic;
                    data["dryRun"] = planDryRun;
                    data["executedCount"] = executed;
                    data["results"] = stepResults;
                    return sendResponse(BuildV2ErrorResponse(409, cmd, "EXECUTE_PLAN_FAILED", "Plan command failed.", requestId, data), "", 409, true);
                }
                executed++;
            }
            nlohmann::json data;
            data["atomic"] = atomic;
            data["dryRun"] = planDryRun;
            data["executedCount"] = executed;
            data["results"] = stepResults;
            return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
        }

        if (currentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }

        long long now = NowEpochMs();
        std::string transactionId = "tx-plan-" + std::to_string(now) + "-" + std::to_string(sequenceElements.GetChangeCount());
        PendingV2Transaction tx;
        tx.id = transactionId;
        tx.sequencePath = BuildCurrentSequencePath(currentSeqXmlFile);
        tx.initialRevision = BuildSequenceRevisionToken(currentSeqXmlFile, sequenceElements);
        tx.createdEpochMs = now;
        tx.expiresEpochMs = now + kV2TransactionTtlMs;
        gPendingV2Transactions[transactionId] = tx;

        int stagedOrExecuted = 0;
        for (size_t i = 0; i < parsedCommands.size(); i++) {
            const auto& command = parsedCommands[i];
            std::string childCmd = command["cmd"].get<std::string>();
            nlohmann::json childParamsJson = command.contains("params") && command["params"].is_object() ? command["params"] : nlohmann::json::object();
            nlohmann::json childOptionsJson = command.contains("options") && command["options"].is_object() ? command["options"] : nlohmann::json::object();
            if (IsV2MutatingCommand(childCmd)) {
                childParamsJson["transactionId"] = transactionId;
                childParamsJson.erase("expectedRevision");
            }
            std::string body;
            int status = 0;
            bool stepOk = ExecuteV2ChildCommand(childCmd, childParamsJson, childOptionsJson, requestId, processAutomation, body, status);
            appendStepResult(static_cast<int>(i), childCmd, status, stepOk, body);
            if (!stepOk) {
                gPendingV2Transactions.erase(transactionId);
                nlohmann::json data;
                data["atomic"] = true;
                data["executedCount"] = stagedOrExecuted;
                data["rolledBack"] = true;
                data["results"] = stepResults;
                return sendResponse(BuildV2ErrorResponse(409, cmd, "EXECUTE_PLAN_FAILED", "Plan command failed before commit.", requestId, data), "", 409, true);
            }
            stagedOrExecuted++;
        }

        std::vector<std::string> commitPaths;
        commitPaths.push_back("transactions.commit");
        std::map<std::string, std::string> commitParams;
        commitParams["_API_VERSION"] = "2";
        commitParams["transactionId"] = transactionId;
        if (!requestId.empty()) {
            commitParams["_REQUEST_ID"] = requestId;
        }

        std::string commitBody;
        int commitStatus = 0;
        bool commitOk = false;
        bool callbackCalled = false;
        bool handled = processAutomation(
            commitPaths,
            commitParams,
            [&](const std::string& msg, const std::string& jsonKey, int responseCode, bool msgIsJSON) {
                callbackCalled = true;
                commitStatus = responseCode;
                if (msgIsJSON) {
                    commitBody = msg;
                } else {
                    nlohmann::json fallback;
                    fallback["res"] = responseCode;
                    fallback[jsonKey.empty() ? "msg" : jsonKey] = msg;
                    commitBody = fallback.dump();
                }
                commitOk = responseCode >= 200 && responseCode < 300;
                return true;
            });
        if (!handled && !callbackCalled) {
            commitStatus = 500;
            commitBody = BuildV2ErrorResponse(500, "transactions.commit", "INTERNAL_ERROR", "Failed to commit executePlan transaction.", requestId);
        }
        appendStepResult(static_cast<int>(parsedCommands.size()), "transactions.commit", commitStatus, commitOk, commitBody);
        if (!commitOk) {
            nlohmann::json data;
            data["atomic"] = true;
            data["executedCount"] = stagedOrExecuted;
            data["rolledBack"] = true;
            data["results"] = stepResults;
            return sendResponse(BuildV2ErrorResponse(409, cmd, "EXECUTE_PLAN_COMMIT_FAILED", "Plan commit failed.", requestId, data), "", 409, true);
        }

        nlohmann::json data;
        data["atomic"] = true;
        data["executedCount"] = stagedOrExecuted;
        data["results"] = stepResults;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "transactions.begin") {
        if (currentSeqXmlFile == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "SEQUENCE_NOT_OPEN", "No sequence open.", requestId), "", 404, true);
        }
        long long now = NowEpochMs();
        std::string transactionId = "tx-" + std::to_string(now) + "-" + std::to_string(sequenceElements.GetChangeCount());
        PendingV2Transaction tx;
        tx.id = transactionId;
        tx.sequencePath = BuildCurrentSequencePath(currentSeqXmlFile);
        tx.initialRevision = BuildSequenceRevisionToken(currentSeqXmlFile, sequenceElements);
        tx.createdEpochMs = now;
        tx.expiresEpochMs = now + kV2TransactionTtlMs;
        gPendingV2Transactions[transactionId] = tx;

        nlohmann::json data;
        data["transactionId"] = transactionId;
        data["sequenceRevision"] = tx.initialRevision;
        data["expiresAtEpochMs"] = tx.expiresEpochMs;
        data["ttlMs"] = kV2TransactionTtlMs;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "transactions.rollback") {
        std::string transactionId = ReadParamString(params, "transactionId");
        if (transactionId.empty() || transactionId == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "transactionId is required.", requestId), "", 422, true);
        }
        auto it = gPendingV2Transactions.find(transactionId);
        if (it == gPendingV2Transactions.end()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRANSACTION_NOT_FOUND", "Transaction not found.", requestId), "", 404, true);
        }
        int dropped = static_cast<int>(it->second.commands.size());
        gPendingV2Transactions.erase(it);
        nlohmann::json data;
        data["transactionId"] = transactionId;
        data["rolledBack"] = true;
        data["droppedCommandCount"] = dropped;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "transactions.commit") {
        std::string transactionId = ReadParamString(params, "transactionId");
        if (transactionId.empty() || transactionId == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "transactionId is required.", requestId), "", 422, true);
        }
        auto it = gPendingV2Transactions.find(transactionId);
        if (it == gPendingV2Transactions.end()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRANSACTION_NOT_FOUND", "Transaction not found.", requestId), "", 404, true);
        }
        PendingV2Transaction& tx = it->second;
        std::string currentPath = BuildCurrentSequencePath(currentSeqXmlFile);
        if (currentPath != tx.sequencePath) {
            gPendingV2Transactions.erase(it);
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRANSACTION_SEQUENCE_CHANGED", "Open sequence changed since transaction begin.", requestId), "", 409, true);
        }

        std::string expectedRevision = ReadParamString(params, "expectedRevision");
        std::string currentRevision = BuildSequenceRevisionToken(currentSeqXmlFile, sequenceElements);
        if (!expectedRevision.empty() && expectedRevision != "null" && expectedRevision != currentRevision) {
            return sendResponse(BuildV2ErrorResponse(409, cmd, "REVISION_CONFLICT", "expectedRevision does not match current sequence revision.", requestId), "", 409, true);
        }

        auto runStaged = [&](const StagedV2Command& staged, bool dryRun, std::string& responseBody, int& statusCode) -> bool {
            std::vector<std::string> childPaths;
            childPaths.push_back(staged.cmd);
            auto childParams = staged.params;
            childParams["_API_VERSION"] = "2";
            childParams.erase("transactionId");
            if (dryRun) {
                childParams["_DRY_RUN"] = "true";
            } else {
                childParams.erase("_DRY_RUN");
            }
            bool callbackCalled = false;
            bool handled = processAutomation(
                childPaths,
                childParams,
                [&](const std::string& msg, const std::string& jsonKey, int responseCode, bool msgIsJSON) {
                    callbackCalled = true;
                    statusCode = responseCode;
                    if (msgIsJSON) {
                        responseBody = msg;
                    } else {
                        nlohmann::json fallback;
                        fallback["res"] = responseCode;
                        fallback[jsonKey.empty() ? "msg" : jsonKey] = msg;
                        responseBody = fallback.dump();
                    }
                    return true;
                });
            if (!handled && !callbackCalled) {
                statusCode = 500;
                responseBody = BuildV2ErrorResponse(500, staged.cmd, "INTERNAL_ERROR", "Failed to execute staged command.", requestId);
                return false;
            }
            return statusCode >= 200 && statusCode < 300;
        };

        int applied = 0;
        for (size_t i = 0; i < tx.commands.size(); i++) {
            std::string body;
            int status = 0;
            if (!runStaged(tx.commands[i], false, body, status)) {
                gPendingV2Transactions.erase(it);
                return sendResponse(
                    BuildV2ErrorResponse(
                        409,
                        cmd,
                        "TRANSACTION_APPLY_FAILED",
                        "Commit failed while applying staged command at index " + std::to_string(i) + " (" + tx.commands[i].cmd +
                            ") after " + std::to_string(applied) + " command(s) applied.",
                        requestId),
                    "",
                    409,
                    true);
            }
            applied++;
        }

        gPendingV2Transactions.erase(it);
        nlohmann::json data;
        data["transactionId"] = transactionId;
        data["committed"] = true;
        data["appliedCommandCount"] = applied;
        data["newRevision"] = BuildSequenceRevisionToken(currentSeqXmlFile, sequenceElements);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    std::string transactionId = ReadParamString(params, "transactionId");
    if (!transactionId.empty() && transactionId != "null" && IsV2MutatingCommand(cmd)) {
        auto it = gPendingV2Transactions.find(transactionId);
        if (it == gPendingV2Transactions.end()) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "TRANSACTION_NOT_FOUND", "Transaction not found.", requestId), "", 404, true);
        }
        PendingV2Transaction& tx = it->second;
        std::string currentPath = BuildCurrentSequencePath(currentSeqXmlFile);
        if (currentPath != tx.sequencePath) {
            gPendingV2Transactions.erase(it);
            return sendResponse(BuildV2ErrorResponse(409, cmd, "TRANSACTION_SEQUENCE_CHANGED", "Open sequence changed since transaction begin.", requestId), "", 409, true);
        }
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        if (dryRun) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "transactionId cannot be combined with dryRun on staged commands.", requestId), "", 422, true);
        }

        StagedV2Command staged;
        staged.cmd = cmd;
        staged.params = params;
        staged.params.erase("transactionId");
        staged.params.erase("expectedRevision");
        staged.params.erase("_REQUEST_ID");
        staged.params.erase("_DRY_RUN");
        tx.commands.push_back(staged);

        nlohmann::json data;
        data["transactionId"] = transactionId;
        data["staged"] = true;
        data["stagedCommandCount"] = static_cast<int>(tx.commands.size());
        data["stagedCmd"] = cmd;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
