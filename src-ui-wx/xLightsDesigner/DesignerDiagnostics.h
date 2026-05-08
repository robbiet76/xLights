#pragma once

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>

namespace xLightsDesigner {
namespace detail {
inline std::mutex& DesignerDiagnosticsMutex() {
    static std::mutex mutex;
    return mutex;
}

inline const char* DesignerDiagnosticsPath() {
    return std::getenv("XLIGHTS_DESIGNER_DIAGNOSTIC_LOG");
}
}

inline void AppendDesignerDiagnostic(const std::string& line) {
    const char* path = detail::DesignerDiagnosticsPath();
    if (path == nullptr || *path == '\0') {
        return;
    }
    std::lock_guard<std::mutex> lock(detail::DesignerDiagnosticsMutex());
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) {
        return;
    }
    out << line << '\n';
}
} // namespace xLightsDesigner
