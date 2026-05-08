#pragma once

#include <string>
#include <vector>

namespace xLightsDesigner::api::validation {

struct ValidationIssue {
    std::string field;
    std::string code;
    std::string message;
};

struct ValidationResult {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool ok() const {
        return issues.empty();
    }

    void addIssue(std::string field, std::string code, std::string message) {
        issues.push_back({std::move(field), std::move(code), std::move(message)});
    }
};

} // namespace xLightsDesigner::api::validation
