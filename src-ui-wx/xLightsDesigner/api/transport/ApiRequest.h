#pragma once

#include <map>
#include <string>

namespace xLightsDesigner::api::transport {

struct ApiRequest {
    std::string command;
    std::string requestId;
    std::map<std::string, std::string> params;
    bool dryRun = false;
};

} // namespace xLightsDesigner::api::transport
