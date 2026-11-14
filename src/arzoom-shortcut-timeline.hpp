#pragma once
#include "arzoom-shortcut-observer.hpp"
#include "arzoom-shortcut-visualizer.hpp"

#include <algorithm>
#include <cstdint>

namespace arzoom::shortcut {

/* Deterministic presentation clock. The hook owns *only* semantic events.
 * OBS video_tick owns age and timeout. Shader owns only final-opacity mix. */
class Timeline {
public:
    static constexpr float kFadeIn = 0.070f;
    static constexpr float kHold = 1.200f;
    static constexpr float kFadeOut = 0.220f;
    static constexpr float kDuration = kFadeIn + kHold + kFadeOut;

    void reset() noexcept
    {
        frame_ = {};
        age_ = kDuration;
        last_generation_ = 0;
    }

    void step(Snapshot latest, float dt) noexcept
    {
        if (latest.generation &&
            latest.generation != last_generation_) {
            last_generation_ = latest.generation;
            frame_ = presentation::build_keyboard_overlay(
                latest.chord, latest.generation);
            age_ = frame_.keycap_count ? 0.0f : kDuration;
        } else {
            age_ = std::min(kDuration, age_ +
                std::clamp(dt, 0.0f, 0.10f));
        }
    }

    bool active() const noexcept
    {
        return frame_.keycap_count != 0 && age_ < kDuration;
    }

    float opacity() const noexcept
    {
        if (!active()) return 0.0f;
        if (age_ < kFadeIn)
            return std::clamp(age_ / kFadeIn, 0.0f, 1.0f);
        if (age_ <= kFadeIn + kHold) return 1.0f;
        return std::clamp(
            (kDuration - age_) / kFadeOut, 0.0f, 1.0f);
    }

    const presentation::KeyboardOverlayFrame &frame() const noexcept
    {
        return frame_;
    }

private:
    presentation::KeyboardOverlayFrame frame_{};
    float age_ = kDuration;
    std::uint32_t last_generation_ = 0;
};

} // namespace arzoom::shortcut
