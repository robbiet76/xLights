#pragma once

#include <string_view>

namespace xLightsDesigner::api::transport::errors {

// Shared error identifiers used by handlers and async job responses.
inline constexpr std::string_view ValidationError = "VALIDATION_ERROR";
inline constexpr std::string_view SequenceNotOpen = "SEQUENCE_NOT_OPEN";
inline constexpr std::string_view SequenceNotFound = "SEQUENCE_NOT_FOUND";
inline constexpr std::string_view InternalError = "INTERNAL_ERROR";

} // namespace xLightsDesigner::api::transport::errors
