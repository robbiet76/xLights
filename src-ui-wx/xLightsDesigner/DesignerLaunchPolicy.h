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
#include <string>

namespace xLightsDesigner {

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

} // namespace xLightsDesigner
