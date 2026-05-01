#pragma once

#include "Common.h"

// ============================================================================
// StateManager
//
// Single source of truth for overlay runtime state.
// All public methods are thread-safe.
// ============================================================================
class StateManager {
public:
    StateManager();

    // --- Click-through ---
    bool IsClickThrough()   const noexcept;
    bool ToggleClickThrough()     noexcept; // returns new value
    void SetClickThrough(bool v)  noexcept;

    // --- Opacity (0-255 scale) ---
    uint8_t GetOpacity()          const noexcept;
    uint8_t ToggleOpacity()             noexcept; // flips LOW/HIGH, returns new value
    void    SetOpacity(uint8_t v)       noexcept;

    // --- Capture exclusion (informational) ---
    bool IsCaptureExcluded()      const noexcept;
    void SetCaptureExcluded(bool v)     noexcept;

private:
    mutable std::mutex mutex_;
    bool    clickThrough_    = true;
    uint8_t opacity_         = OPACITY_HIGH;
    bool    captureExcluded_ = false;
};
