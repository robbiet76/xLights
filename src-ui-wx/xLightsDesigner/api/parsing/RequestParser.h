#pragma once

#include "../transport/ApiRequest.h"
#include "ParameterReaders.h"

namespace xLightsDesigner::api::parsing {

inline transport::ApiRequest ParseRequest(const std::string& command,
                                          const std::map<std::string, std::string>& params,
                                          const std::string& requestId = {}) {
    transport::ApiRequest request;
    request.command = command;
    request.requestId = requestId;
    request.params = params;
    request.dryRun = ReadBool(params, "_DRY_RUN", false);
    return request;
}

} // namespace xLightsDesigner::api::parsing
