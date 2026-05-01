#include "StateManager.h"

StateManager::StateManager()
    : clickThrough_(false)
    , opacity_(OPACITY_HIGH)
    , captureExcluded_(false)
{}

bool StateManager::IsClickThrough() const noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    return clickThrough_;
}

bool StateManager::ToggleClickThrough() noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    clickThrough_ = !clickThrough_;
    return clickThrough_;
}

void StateManager::SetClickThrough(bool v) noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    clickThrough_ = v;
}

uint8_t StateManager::GetOpacity() const noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    return opacity_;
}

uint8_t StateManager::ToggleOpacity() noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    opacity_ = (opacity_ >= OPACITY_HIGH) ? OPACITY_LOW : OPACITY_HIGH;
    return opacity_;
}

void StateManager::SetOpacity(uint8_t v) noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    opacity_ = v;
}

bool StateManager::IsCaptureExcluded() const noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    return captureExcluded_;
}

void StateManager::SetCaptureExcluded(bool v) noexcept {
    std::lock_guard<std::mutex> lk(mutex_);
    captureExcluded_ = v;
}
