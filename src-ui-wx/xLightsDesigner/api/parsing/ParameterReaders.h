#pragma once

#include <cstdlib>
#include <map>
#include <string>

namespace xLightsDesigner::api::parsing {

// Shared primitive readers for flattened request params. Invalid values fall
// back instead of throwing so handlers can decide whether validation is needed.
inline std::string ReadString(const std::map<std::string, std::string>& params,
                              const std::string& key,
                              const std::string& fallback = {}) {
    auto it = params.find(key);
    return it == params.end() ? fallback : it->second;
}

inline bool ReadBool(const std::map<std::string, std::string>& params,
                     const std::string& key,
                     bool fallback = false) {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    return it->second == "1" || it->second == "true" || it->second == "TRUE";
}

inline int ReadInt(const std::map<std::string, std::string>& params,
                   const std::string& key,
                   int fallback = 0) {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    char* end = nullptr;
    long value = std::strtol(it->second.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return fallback;
    }
    return static_cast<int>(value);
}

} // namespace xLightsDesigner::api::parsing
