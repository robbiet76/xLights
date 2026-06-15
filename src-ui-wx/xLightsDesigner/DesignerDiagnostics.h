#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace xLightsDesigner {
namespace detail {
// Diagnostics are intentionally file-backed and bounded so noninteractive runs
// can report suppressed prompts without requiring an attached UI.
inline std::mutex& DesignerDiagnosticsMutex() {
    static std::mutex mutex;
    return mutex;
}

inline const char* DesignerDiagnosticsPath() {
    return std::getenv("XLIGHTS_DESIGNER_DIAGNOSTIC_LOG");
}

inline std::uint64_t NextSuppressedDialogId() {
    static std::atomic<std::uint64_t> nextId{1};
    return nextId.fetch_add(1, std::memory_order_relaxed);
}

inline std::string DiagnosticNowUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&time, &utc);
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

inline std::deque<nlohmann::json>& SuppressedDialogs() {
    static std::deque<nlohmann::json> dialogs;
    return dialogs;
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

inline void RecordSuppressedDialog(const std::string& level,
                                   const std::string& source,
                                   const std::string& title,
                                   const std::string& message) {
    constexpr std::size_t maxSuppressedDialogs = 50;
    nlohmann::json event{
        {"id", detail::NextSuppressedDialogId()},
        {"at", detail::DiagnosticNowUtcIso8601()},
        {"level", level},
        {"source", source},
        {"title", title},
        {"message", message}
    };

    std::lock_guard<std::mutex> lock(detail::DesignerDiagnosticsMutex());
    auto& dialogs = detail::SuppressedDialogs();
    dialogs.push_back(event);
    while (dialogs.size() > maxSuppressedDialogs) {
        dialogs.pop_front();
    }

    const char* path = detail::DesignerDiagnosticsPath();
    if (path != nullptr && *path != '\0') {
        std::ofstream out(path, std::ios::app);
        if (out.is_open()) {
            out << "suppressedDialog " << event.dump() << '\n';
        }
    }
}

inline nlohmann::json GetSuppressedDialogsJson() {
    std::lock_guard<std::mutex> lock(detail::DesignerDiagnosticsMutex());
    return nlohmann::json(detail::SuppressedDialogs());
}
} // namespace xLightsDesigner
