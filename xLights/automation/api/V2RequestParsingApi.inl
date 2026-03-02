static bool ReadScalarParam(const nlohmann::json& value, std::string& out) {
    if (value.is_string()) {
        out = value.get<std::string>();
        return true;
    }
    if (value.is_boolean()) {
        out = value.get<bool>() ? "true" : "false";
        return true;
    }
    if (value.is_number_unsigned()) {
        out = std::to_string(value.get<uint64_t>());
        return true;
    }
    if (value.is_number_integer()) {
        out = std::to_string(value.get<int64_t>());
        return true;
    }
    if (value.is_number_float()) {
        out = std::to_string(value.get<double>());
        return true;
    }
    return false;
}

static bool ParseXlDoAutomationBody(const std::string& body,
                                    std::vector<std::string>& paths,
                                    std::map<std::string, std::string>& paramMap,
                                    std::string& errorBody,
                                    int& errorStatus) {
    nlohmann::json val;
    try {
        val = nlohmann::json::parse(body);
    } catch (const std::exception&) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Malformed JSON request body.");
        return false;
    }

    if (!val.is_object()) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Request body must be a JSON object.");
        return false;
    }

    try {
        if (val.contains("apiVersion")) {
            if (!val["apiVersion"].is_number_integer()) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "apiVersion must be an integer.");
                return false;
            }
            int apiVersion = val["apiVersion"].get<int>();
            if (apiVersion != 2) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "UNSUPPORTED_API_VERSION", "Only apiVersion=2 is supported.");
                return false;
            }

            if (!val.contains("cmd") || !val["cmd"].is_string() || val["cmd"].get<std::string>().empty()) {
                errorStatus = 400;
                errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Missing cmd.");
                return false;
            }

            paths.push_back(val["cmd"].get<std::string>());
            paramMap["_API_VERSION"] = "2";

            if (val.contains("options")) {
                if (!val["options"].is_object()) {
                    errorStatus = 400;
                    errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options must be an object.");
                    return false;
                }

                auto options = val["options"];
                if (options.contains("requestId")) {
                    if (!options["requestId"].is_string()) {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options.requestId must be a string.");
                        return false;
                    }
                    paramMap["_REQUEST_ID"] = options["requestId"].get<std::string>();
                }
                if (options.contains("dryRun")) {
                    if (options["dryRun"].is_boolean()) {
                        paramMap["_DRY_RUN"] = options["dryRun"].get<bool>() ? "true" : "false";
                    } else if (options["dryRun"].is_number_integer()) {
                        paramMap["_DRY_RUN"] = options["dryRun"].get<int>() != 0 ? "true" : "false";
                    } else {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "options.dryRun must be a boolean or integer.");
                        return false;
                    }
                }
            }

            if (val.contains("params")) {
                if (!val["params"].is_object()) {
                    errorStatus = 400;
                    errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params must be an object.");
                    return false;
                }

                for (auto [name, value] : val["params"].items()) {
                    if (name == "commands" && value.is_array()) {
                        paramMap[name] = value.dump();
                        continue;
                    }

                    if (value.is_array()) {
                        for (size_t i = 0; i < value.size(); i++) {
                            if (value[i].is_object()) {
                                for (auto [childName, childValue] : value[i].items()) {
                                    std::string scalar;
                                    if (!ReadScalarParam(childValue, scalar)) {
                                        errorStatus = 400;
                                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params object-array values must be string, number, or boolean.");
                                        return false;
                                    }
                                    paramMap[name + "_" + std::to_string(i) + "_" + childName] = scalar;
                                }
                                continue;
                            }
                            std::string scalar;
                            if (!ReadScalarParam(value[i], scalar)) {
                                errorStatus = 400;
                                errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params values must be string, number, or boolean.");
                                return false;
                            }
                            paramMap[name + "_" + std::to_string(i)] = scalar;
                        }
                        continue;
                    }

                    if (value.is_object()) {
                        paramMap[name] = value.dump();
                        continue;
                    }

                    std::string scalar;
                    if (!ReadScalarParam(value, scalar)) {
                        errorStatus = 400;
                        errorBody = BuildV2ErrorResponse(400, paths[0], "BAD_REQUEST", "params values must be string, number, or boolean.");
                        return false;
                    }
                    paramMap[name] = scalar;
                }
            }

            paramMap["_METHOD"] = "POST";
            return true;
        }

        if (!val.contains("cmd")) {
            errorStatus = 503;
            errorBody = "{\"res\":503,\"msg\":\"Missing cmd.\"}";
            return false;
        }
        if (!val["cmd"].is_string() || val["cmd"].get<std::string>().empty()) {
            errorStatus = 400;
            errorBody = "{\"res\":400,\"msg\":\"cmd must be a non-empty string.\"}";
            return false;
        }

        paths.push_back(val["cmd"].get<std::string>());
        for (auto [name, value] : val.items()) {
            if (name == "cmd") {
                continue;
            }

            if (value.is_array()) {
                for (size_t x = 0; x < value.size(); x++) {
                    std::string scalar;
                    if (!ReadScalarParam(value[x], scalar)) {
                        errorStatus = 400;
                        errorBody = "{\"res\":400,\"msg\":\"Array params must contain string, number, or boolean values.\"}";
                        return false;
                    }
                    paramMap[name + "_" + std::to_string(x)] = scalar;
                }
            } else {
                std::string scalar;
                if (!ReadScalarParam(value, scalar)) {
                    errorStatus = 400;
                    errorBody = "{\"res\":400,\"msg\":\"Params must be string, number, boolean, or arrays of those values.\"}";
                    return false;
                }
                paramMap[name] = scalar;
            }
        }
    } catch (const std::exception&) {
        errorStatus = 400;
        errorBody = BuildV2ErrorResponse(400, "", "BAD_REQUEST", "Request contains values that are out of supported range.");
        return false;
    }

    paramMap["_METHOD"] = paramMap.empty() ? "GET" : "POST";
    return true;
}
