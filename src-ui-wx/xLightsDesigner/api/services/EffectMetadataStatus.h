#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "shared/utils/wxUtilities.h"

namespace xLightsDesigner::api::services {

struct EffectMetadataStatus {
    std::string metadataDir;
    bool metadataDirExists{false};
    bool schemaPresent{false};
    int effectFileCount{0};
    int sharedFileCount{0};
    std::string schemaHash;
    std::string bundleFingerprint;
    nlohmann::json files{nlohmann::json::array()};
};

namespace effect_metadata_status_detail {

inline std::string ReadAllText(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

inline uint64_t Fnv1a64(const std::string& value) {
    constexpr uint64_t offset = 1469598103934665603ull;
    constexpr uint64_t prime = 1099511628211ull;
    uint64_t hash = offset;
    for (unsigned char ch : value) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= prime;
    }
    return hash;
}

inline std::string ToHex(uint64_t value) {
    static const char* digits = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = digits[value & 0xF];
        value >>= 4;
    }
    return out;
}

inline std::string HashText(const std::string& value) {
    return ToHex(Fnv1a64(value));
}

inline bool IsJsonFile(const std::filesystem::path& path) {
    return path.extension() == ".json";
}

} // namespace effect_metadata_status_detail

inline EffectMetadataStatus GetEffectMetadataStatus() {
    using namespace effect_metadata_status_detail;

    EffectMetadataStatus status;
    status.metadataDir = GetEffectMetadataDirectory();
    if (status.metadataDir.empty()) {
        return status;
    }

    std::error_code ec;
    const std::filesystem::path root(status.metadataDir);
    status.metadataDirExists = std::filesystem::is_directory(root, ec);
    if (!status.metadataDirExists) {
        return status;
    }

    std::vector<std::string> fingerprintParts;
    const auto schemaPath = root / "_schema.json";
    if (std::filesystem::is_regular_file(schemaPath, ec)) {
        status.schemaPresent = true;
        status.schemaHash = HashText(ReadAllText(schemaPath.string()));
        fingerprintParts.push_back(std::string("_schema.json:") + status.schemaHash);
        status.files.push_back({{"path", "_schema.json"}, {"hash", status.schemaHash}});
    }

    std::vector<std::filesystem::path> effectFiles;
    std::vector<std::filesystem::path> sharedFiles;
    const auto sharedDir = root / "shared";

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (!IsJsonFile(entry.path())) continue;
        if (entry.path().filename() == "_schema.json") continue;
        effectFiles.push_back(entry.path());
    }
    if (std::filesystem::is_directory(sharedDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(sharedDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (!IsJsonFile(entry.path())) continue;
            sharedFiles.push_back(entry.path());
        }
    }

    auto sortByName = [](const std::filesystem::path& left, const std::filesystem::path& right) {
        return left.filename().string() < right.filename().string();
    };
    std::sort(effectFiles.begin(), effectFiles.end(), sortByName);
    std::sort(sharedFiles.begin(), sharedFiles.end(), sortByName);

    status.effectFileCount = static_cast<int>(effectFiles.size());
    status.sharedFileCount = static_cast<int>(sharedFiles.size());

    for (const auto& path : effectFiles) {
        const auto hash = HashText(ReadAllText(path.string()));
        const auto name = path.filename().string();
        fingerprintParts.push_back(name + ":" + hash);
        status.files.push_back({{"path", name}, {"hash", hash}});
    }
    for (const auto& path : sharedFiles) {
        const auto hash = HashText(ReadAllText(path.string()));
        const auto name = std::string("shared/") + path.filename().string();
        fingerprintParts.push_back(name + ":" + hash);
        status.files.push_back({{"path", name}, {"hash", hash}});
    }

    std::string joined;
    for (const auto& part : fingerprintParts) {
        joined += part;
        joined.push_back('\n');
    }
    status.bundleFingerprint = HashText(joined);
    return status;
}

} // namespace xLightsDesigner::api::services
