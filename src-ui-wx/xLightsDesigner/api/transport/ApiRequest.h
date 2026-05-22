#pragma once

#include <map>
#include <string>

namespace xLightsDesigner::api::transport {

// Normalized command request passed from HTTP routing into typed handlers.
// Query-string values and JSON body fields are flattened before this point so
// handlers can share one parser path across GET and POST calls.
struct ApiRequest {
    std::string command;
    std::string requestId;
    std::map<std::string, std::string> params;
    bool dryRun = false;
};

} // namespace xLightsDesigner::api::transport
