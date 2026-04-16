#pragma once

namespace xLightsDesigner {
namespace detail {
inline bool& SuppressCloseSequenceSavePrompt() {
    static bool enabled = false;
    return enabled;
}

inline bool& SuppressUnsavedShowDirectoryPrompts() {
    static bool enabled = false;
    return enabled;
}
} // namespace detail

inline bool ShouldSuppressDesignerCloseSequenceSavePrompt() {
    return detail::SuppressCloseSequenceSavePrompt();
}

inline bool ShouldSuppressDesignerUnsavedShowDirectoryPrompts() {
    return detail::SuppressUnsavedShowDirectoryPrompts();
}

class ScopedDesignerCloseSequenceSavePromptSuppression {
public:
    ScopedDesignerCloseSequenceSavePromptSuppression()
        : _previous(detail::SuppressCloseSequenceSavePrompt()) {
        detail::SuppressCloseSequenceSavePrompt() = true;
    }

    ~ScopedDesignerCloseSequenceSavePromptSuppression() {
        detail::SuppressCloseSequenceSavePrompt() = _previous;
    }

private:
    bool _previous = false;
};

class ScopedDesignerUnsavedShowDirectoryPromptSuppression {
public:
    ScopedDesignerUnsavedShowDirectoryPromptSuppression()
        : _previous(detail::SuppressUnsavedShowDirectoryPrompts()) {
        detail::SuppressUnsavedShowDirectoryPrompts() = true;
    }

    ~ScopedDesignerUnsavedShowDirectoryPromptSuppression() {
        detail::SuppressUnsavedShowDirectoryPrompts() = _previous;
    }

private:
    bool _previous = false;
};
} // namespace xLightsDesigner
