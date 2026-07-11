#pragma once

#include <utility>

#include <nlohmann/json.hpp>

#include "../../DesignerApiRuntime.h"
#include "../parsing/ParameterReaders.h"
#include "../services/ElementService.h"
#include "../transport/ApiRequest.h"
#include "../transport/ApiResponse.h"
#include "../transport/ErrorCatalog.h"

namespace xLightsDesigner::api::handlers {

// Element endpoints expose sequence row inventory, ordering, and selected
// display rows. Native effect content is handled by EffectHandler.
class ElementHandler {
public:
    explicit ElementHandler(services::ElementService service)
        : _service(std::move(service)) {}

    [[nodiscard]] transport::ApiResponse handleGetSummary(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getSummary();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{
                std::string(transport::errors::SequenceNotOpen),
                "No sequence open.",
                nlohmann::json::object()
            };
            return response;
        }

        response.data["elements"] = nlohmann::json::array();
        for (const auto& element : summary.elements) {
            nlohmann::json jsonElement = {
                {"name", element.name},
                {"type", element.type},
                {"selected", element.selected},
                {"visible", element.visible},
                {"totalEffectCount", element.totalEffectCount},
                {"layers", nlohmann::json::array()}
            };
            for (const auto& layer : element.layers) {
                jsonElement["layers"].push_back({
                    {"layerNumber", layer.layerNumber},
                    {"effectCount", layer.effectCount},
                    {"layerName", layer.layerName}
                });
            }
            response.data["elements"].push_back(std::move(jsonElement));
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleGetSelectedDisplayElements(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getSelectedDisplayElements();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }

        response.data["selectedElementNames"] = summary.selectedElementNames;
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleEnsureSequenceElements(const transport::ApiRequest& request) const {
        const auto elementNamesText = parsing::ReadString(request.params, "elementNames");
        if (elementNamesText.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.ensure requires elementNames JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json elementNamesJson;
        try {
            elementNamesJson = nlohmann::json::parse(elementNamesText);
        } catch (const std::exception& ex) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.ensure received invalid elementNames JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!elementNamesJson.is_array() || elementNamesJson.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.ensure requires elementNames to be a non-empty JSON array.", nlohmann::json::object()};
            return response;
        }

        models::EnsureSequenceElementsRequest ensureRequest;
        for (const auto& item : elementNamesJson) {
            if (item.is_string()) {
                ensureRequest.elementNames.push_back(item.get<std::string>());
            }
        }
        if (ensureRequest.elementNames.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.ensure did not receive any usable element names.", nlohmann::json::object()};
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, ensureRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.ensureSequenceElements(ensureRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404;
                queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400;
                queuedResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("elements.ensure failed."),
                    {{"missingNames", result.missingNames}}
                };
                return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["addedCount"] = result.addedCount;
            queuedResponse.data["addedNames"] = result.addedNames;
            queuedResponse.data["existingNames"] = result.existingNames;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleSetSelectedDisplayElements(const transport::ApiRequest& request) const {
        const auto elementNamesText = parsing::ReadString(request.params, "elementNames");
        if (elementNamesText.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setSelected requires elementNames JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json elementNamesJson;
        try {
            elementNamesJson = nlohmann::json::parse(elementNamesText);
        } catch (const std::exception& ex) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setSelected received invalid elementNames JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!elementNamesJson.is_array()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setSelected requires elementNames to be a JSON array.", nlohmann::json::object()};
            return response;
        }

        models::SetSelectedDisplayElementsRequest selectionRequest;
        selectionRequest.replaceExisting = parsing::ReadBool(request.params, "replaceExisting", true);
        for (const auto& item : elementNamesJson) {
            if (item.is_string()) {
                selectionRequest.elementNames.push_back(item.get<std::string>());
            }
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, selectionRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.setSelectedDisplayElements(selectionRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404;
                queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400;
                queuedResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("elements.setSelected failed."),
                    {{"missingNames", result.missingNames}}
                };
                return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["selectedCount"] = result.selectedCount;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleSetDisplayElementVisibility(const transport::ApiRequest& request) const {
        const auto visibleNamesText = parsing::ReadString(request.params, "visibleElementNames");
        if (visibleNamesText.empty()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setVisibility requires visibleElementNames JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json visibleNamesJson;
        try {
            visibleNamesJson = nlohmann::json::parse(visibleNamesText);
        } catch (const std::exception& ex) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setVisibility received invalid visibleElementNames JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!visibleNamesJson.is_array()) {
            transport::ApiResponse response; response.command = request.command; response.requestId = request.requestId; response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setVisibility requires visibleElementNames to be a JSON array.", nlohmann::json::object()};
            return response;
        }

        models::SetDisplayElementVisibilityRequest visibilityRequest;
        visibilityRequest.hideUnlistedModels = parsing::ReadBool(request.params, "hideUnlistedModels", true);
        for (const auto& item : visibleNamesJson) {
            if (item.is_string()) {
                visibilityRequest.visibleElementNames.push_back(item.get<std::string>());
            }
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, visibilityRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.setDisplayElementVisibility(visibilityRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404;
                queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400;
                queuedResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("elements.setVisibility failed."),
                    {{"missingNames", result.missingNames}}
                };
                return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["visibleCount"] = result.visibleCount;
            queuedResponse.data["hiddenCount"] = result.hiddenCount;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

    [[nodiscard]] transport::ApiResponse handleGetDisplayOrder(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto summary = _service.getDisplayOrder();
        if (!summary.sequenceOpen) {
            response.statusCode = 404;
            response.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
            return response;
        }

        response.data["elements"] = nlohmann::json::array();
        for (const auto& element : summary.elements) {
            response.data["elements"].push_back({
                {"id", element.id},
                {"type", element.type},
                {"orderIndex", element.orderIndex}
            });
        }
        return response;
    }

    [[nodiscard]] transport::ApiResponse handleSetDisplayOrder(const transport::ApiRequest& request) const {
        transport::ApiResponse response;
        response.command = request.command;
        response.requestId = request.requestId;

        const auto orderedIdsText = parsing::ReadString(request.params, "orderedIds");
        if (orderedIdsText.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setDisplayOrder requires orderedIds JSON.", nlohmann::json::object()};
            return response;
        }

        nlohmann::json orderedIdsJson;
        try {
            orderedIdsJson = nlohmann::json::parse(orderedIdsText);
        } catch (const std::exception& ex) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setDisplayOrder received invalid orderedIds JSON.", {{"reason", ex.what()}}};
            return response;
        }
        if (!orderedIdsJson.is_array() || orderedIdsJson.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setDisplayOrder requires orderedIds to be a non-empty JSON array.", nlohmann::json::object()};
            return response;
        }

        models::SetDisplayElementOrderRequest orderRequest;
        for (const auto& item : orderedIdsJson) {
            if (item.is_string()) {
                orderRequest.orderedIds.push_back(item.get<std::string>());
            } else if (item.is_number_integer()) {
                orderRequest.orderedIds.push_back(std::to_string(item.get<int>()));
            }
        }
        if (orderRequest.orderedIds.empty()) {
            response.statusCode = 400;
            response.error = transport::ApiError{std::string(transport::errors::ValidationError), "elements.setDisplayOrder did not receive any usable ids.", nlohmann::json::object()};
            return response;
        }

        const auto jobId = SubmitDesignerApiJob(request.command, request.requestId, [service = _service, request, orderRequest]() {
            transport::ApiResponse queuedResponse;
            queuedResponse.command = request.command;
            queuedResponse.requestId = request.requestId;
            const auto result = service.setDisplayOrder(orderRequest);
            if (!result.sequenceOpen) {
                queuedResponse.statusCode = 404;
                queuedResponse.error = transport::ApiError{std::string(transport::errors::SequenceNotOpen), "No sequence open.", nlohmann::json::object()};
                return queuedResponse;
            }
            if (!result.ok) {
                queuedResponse.statusCode = 400;
                queuedResponse.error = transport::ApiError{
                    result.errorCode.value_or(std::string(transport::errors::ValidationError)),
                    result.errorMessage.value_or("elements.setDisplayOrder failed."),
                    {{"missingIds", result.missingIds}, {"duplicateIds", result.duplicateIds}}
                };
                return queuedResponse;
            }
            queuedResponse.data["ok"] = true;
            queuedResponse.data["orderedCount"] = result.orderedCount;
            return queuedResponse;
        });
        return BuildQueuedJobAcceptedResponse(request, jobId);
    }

private:
    services::ElementService _service;
};

} // namespace xLightsDesigner::api::handlers
