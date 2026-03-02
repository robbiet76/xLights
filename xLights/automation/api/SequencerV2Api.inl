namespace automation::api {

static std::optional<bool> HandleSequencerV2Command(
    SequenceElements& sequenceElements,
    const std::function<std::optional<bool>()>& requireOpenSequence,
    const std::function<void()>& refreshEffectGrid,
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "sequencer.getDisplayElementOrder") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        nlohmann::json data;
        data["elements"] = BuildDisplayElementOrderData(sequenceElements);
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    if (cmd == "sequencer.setDisplayElementOrder") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }
        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        std::vector<std::string> orderedIds = ReadParamArray(params, "orderedIds");
        if (orderedIds.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "orderedIds is required.", requestId), "", 422, true);
        }

        size_t elementCount = sequenceElements.GetElementCount(MASTER_VIEW);
        if (orderedIds.size() != elementCount) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "orderedIds must include all display elements.", requestId), "", 422, true);
        }

        std::vector<std::string> currentOrder;
        currentOrder.reserve(elementCount);
        std::map<std::string, int> indexById;
        for (size_t i = 0; i < elementCount; i++) {
            Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
            if (element == nullptr) {
                continue;
            }
            currentOrder.push_back(element->GetName());
            indexById[element->GetName()] = static_cast<int>(i);
        }

        std::set<std::string> seenIds;
        for (const auto& id : orderedIds) {
            if (indexById.find(id) == indexById.end()) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "DISPLAY_ELEMENT_NOT_FOUND", "Display element not found: '" + id + "'.", requestId), "", 404, true);
            }
            if (!seenIds.insert(id).second) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "orderedIds contains duplicates.", requestId), "", 422, true);
            }
        }

        if (!dryRun) {
            for (size_t targetIndex = 0; targetIndex < orderedIds.size(); targetIndex++) {
                const std::string& desiredId = orderedIds[targetIndex];
                int foundIndex = -1;
                for (size_t i = targetIndex; i < currentOrder.size(); i++) {
                    if (currentOrder[i] == desiredId) {
                        foundIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (foundIndex == -1) {
                    return sendResponse(BuildV2ErrorResponse(500, cmd, "INTERNAL_ERROR", "Unable to apply display element ordering.", requestId), "", 500, true);
                }
                if (foundIndex != static_cast<int>(targetIndex)) {
                    sequenceElements.MoveSequenceElement(foundIndex, static_cast<int>(targetIndex), MASTER_VIEW);
                    std::string moved = currentOrder[foundIndex];
                    currentOrder.erase(currentOrder.begin() + foundIndex);
                    currentOrder.insert(currentOrder.begin() + static_cast<int>(targetIndex), moved);
                }
            }
            sequenceElements.PopulateRowInformation();
            sequenceElements.PopulateVisibleRowInformation();
            refreshEffectGrid();
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);

        nlohmann::json data;
        data["updated"] = true;
        data["elementCount"] = static_cast<int>(elementCount);
        if (dryRun) {
            nlohmann::json projected = nlohmann::json::array();
            for (size_t i = 0; i < orderedIds.size(); i++) {
                const std::string& id = orderedIds[i];
                Element* element = sequenceElements.GetElement(id);
                projected.push_back({
                    {"id", id},
                    {"name", id},
                    {"type", GetElementTypeName(element)},
                    {"orderIndex", static_cast<int>(i)}
                });
            }
            data["elements"] = projected;
        } else {
            data["elements"] = BuildDisplayElementOrderData(sequenceElements);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
