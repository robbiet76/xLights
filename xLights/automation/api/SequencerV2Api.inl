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

    if (cmd == "sequencer.setActiveDisplayElements") {
        if (auto response = requireOpenSequence()) {
            return *response;
        }

        bool dryRun = ReadBool(ReadParamString(params, "_DRY_RUN", "false"));
        std::vector<std::string> activeIds = ReadParamArray(params, "activeIds");
        bool preserveRelativeOrder = true;
        if (params.find("preserveRelativeOrder") != params.end()) {
            preserveRelativeOrder = ReadBool(ReadParamString(params, "preserveRelativeOrder", "true"));
        }
        if (activeIds.empty()) {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "activeIds is required.", requestId), "", 422, true);
        }

        std::set<std::string> seenActiveIds;
        std::set<std::string> activeSet;
        for (const auto& id : activeIds) {
            if (id.empty()) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "activeIds entries must be non-empty strings.", requestId), "", 422, true);
            }
            if (!seenActiveIds.insert(id).second) {
                return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "activeIds contains duplicates.", requestId), "", 422, true);
            }
            activeSet.insert(id);
        }

        std::map<std::string, Element*> elementsById;
        std::set<std::string> ambiguousIds;
        size_t elementCount = sequenceElements.GetElementCount(MASTER_VIEW);
        for (size_t i = 0; i < elementCount; i++) {
            Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
            if (element == nullptr || element->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
                continue;
            }
            const std::string& id = element->GetName();
            if (id.empty()) {
                continue;
            }
            if (elementsById.find(id) != elementsById.end()) {
                ambiguousIds.insert(id);
            } else {
                elementsById[id] = element;
            }
        }

        for (const auto& id : activeIds) {
            if (ambiguousIds.find(id) != ambiguousIds.end()) {
                return sendResponse(BuildV2ErrorResponse(409, cmd, "DISPLAY_ELEMENT_AMBIGUOUS", "Display element id is ambiguous: '" + id + "'.", requestId), "", 409, true);
            }
            if (elementsById.find(id) == elementsById.end()) {
                return sendResponse(BuildV2ErrorResponse(404, cmd, "DISPLAY_ELEMENT_NOT_FOUND", "Display element not found: '" + id + "'.", requestId), "", 404, true);
            }
        }

        bool updated = false;
        if (!dryRun) {
            for (size_t i = 0; i < elementCount; i++) {
                Element* element = sequenceElements.GetElement(i, MASTER_VIEW);
                if (element == nullptr || element->GetType() == ElementType::ELEMENT_TYPE_TIMING) {
                    continue;
                }
                bool shouldBeVisible = activeSet.find(element->GetName()) != activeSet.end();
                if (element->GetVisible() != shouldBeVisible) {
                    element->SetVisible(shouldBeVisible);
                    updated = true;
                }
            }

            if (updated) {
                sequenceElements.PopulateRowInformation();
                sequenceElements.PopulateVisibleRowInformation();
                refreshEffectGrid();
            }
        } else {
            updated = true;
        }

        nlohmann::json warnings = BuildDryRunWarnings(dryRun);
        if (!preserveRelativeOrder) {
            warnings.push_back({
                {"code", "ORDER_UNCHANGED"},
                {"message", "Display element ordering is unchanged. Use sequencer.setDisplayElementOrder to reorder."}
            });
        }

        nlohmann::json data;
        data["updated"] = updated;
        data["activeCount"] = static_cast<int>(activeIds.size());
        nlohmann::json activeIdsOut = nlohmann::json::array();
        for (const auto& id : activeIds) {
            activeIdsOut.push_back(id);
        }
        data["activeIds"] = activeIdsOut;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId, warnings), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
