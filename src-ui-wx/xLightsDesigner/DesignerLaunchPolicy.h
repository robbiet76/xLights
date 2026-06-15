#pragma once

/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace xLightsDesigner {

// Launch policy centralizes env-gated behavior for noninteractive XLD sessions.
// Normal xLights launches do not pass these flags and keep baseline behavior.
inline bool IsTruthyLaunchFlagValue(const char* value)
{
    if (value == nullptr) {
        return false;
    }
    const std::string enabled(value);
    return enabled == "1" || enabled == "true" || enabled == "TRUE" || enabled == "yes" || enabled == "YES";
}

inline bool IsNonInteractiveLaunch()
{
    return IsTruthyLaunchFlagValue(std::getenv("XLIGHTS_DESIGNER_ENABLED"));
}

inline std::string ModalPolicy()
{
    const char* value = std::getenv("XLIGHTS_DESIGNER_MODAL_POLICY");
    if (value == nullptr || *value == '\0') {
        return "safe";
    }
    return value;
}

inline bool ShouldUseAutosaveBackup()
{
    const std::string policy = ModalPolicy();
    return policy == "save";
}

inline bool ShouldSuppressPrompt()
{
    return IsNonInteractiveLaunch();
}

inline std::filesystem::path ResolveLaunchAccessTarget(const std::string& path)
{
    std::error_code ec;
    const std::filesystem::path fsPath(path);
    if (path.empty()) {
        return std::filesystem::path();
    }
    if (std::filesystem::exists(fsPath, ec)) {
        return std::filesystem::weakly_canonical(fsPath, ec);
    }
    const auto parent = fsPath.parent_path();
    if (parent.empty() || !std::filesystem::exists(parent, ec)) {
        return std::filesystem::path();
    }
    return std::filesystem::weakly_canonical(parent, ec) / fsPath.filename();
}

inline bool IsLaunchPathWithinRoot(const std::filesystem::path& target, const std::filesystem::path& root)
{
    auto targetIt = target.begin();
    auto rootIt = root.begin();
    for (; rootIt != root.end(); ++rootIt, ++targetIt) {
        if (targetIt == target.end() || *targetIt != *rootIt) {
            return false;
        }
    }
    return true;
}

inline bool IsLaunchTrustedRootPath(const std::string& path)
{
    const auto target = ResolveLaunchAccessTarget(path);
    if (target.empty()) {
        return false;
    }

    const char* rawRoots = std::getenv("XLIGHTS_DESIGNER_TRUSTED_ROOTS");
    if (rawRoots == nullptr || *rawRoots == '\0') {
        return false;
    }

    std::stringstream stream(rawRoots);
    std::string rootEntry;
    while (std::getline(stream, rootEntry, ':')) {
        if (rootEntry.empty()) {
            continue;
        }
        const auto root = ResolveLaunchAccessTarget(rootEntry);
        if (!root.empty() && IsLaunchPathWithinRoot(target, root)) {
            return true;
        }
    }
    return false;
}

inline bool HasLaunchTrustedRootAccess(const std::string& path, bool enforceWritable)
{
    if (!IsNonInteractiveLaunch() || path.empty() || !IsLaunchTrustedRootPath(path)) {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path fsPath(path);
    const auto existingTarget = std::filesystem::exists(fsPath, ec) ? fsPath : fsPath.parent_path();
    if (ec || existingTarget.empty() || !std::filesystem::exists(existingTarget, ec)) {
        return false;
    }

    if (!enforceWritable) {
        return true;
    }

    const auto probeDir = std::filesystem::is_directory(existingTarget, ec) ? existingTarget : existingTarget.parent_path();
    if (ec || probeDir.empty() || !std::filesystem::exists(probeDir, ec)) {
        return false;
    }

    const auto probe = probeDir / ".xld-owned-write-test";
    std::ofstream out(probe.string(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.close();
    std::filesystem::remove(probe, ec);
    return true;
}

} // namespace xLightsDesigner
