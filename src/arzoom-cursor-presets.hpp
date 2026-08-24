#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace arzoom {

enum class BuiltinCursorVisual {
    Prism,
    Outline,
    Azure,
    Orchid,
};

struct CursorPreset {
    const char *id;
    const char *locale_key;
    BuiltinCursorVisual visual;
    size_t frame_count;
    float play_seconds;
    float hotspot_x;
    float hotspot_y;
    float recommended_size_px;
};

inline constexpr const char *kCursorStyleOff = "off";
inline constexpr const char *kCursorStyleCustom = "custom";

/*
 * Built-in click gestures are intentionally short micro-interactions rather
 * than decorative animations. The visual contract is:
 *
 *   immediate press -> elastic release -> tiny rebound -> settle
 *
 * Outline is the snappiest professional preset; Prism/Orchid keep a few extra
 * milliseconds for their richer fills while staying comfortably below 300ms.
 */
inline constexpr std::array<CursorPreset, 4> kCursorPresets{{
    {"prism", "ArZoom.PresentationCursor.Preset.Prism",
     BuiltinCursorVisual::Prism, 28, 0.280f, 0.19375f, 0.10625f, 52.0f},
    {"outline", "ArZoom.PresentationCursor.Preset.Outline",
     BuiltinCursorVisual::Outline, 28, 0.220f, 0.19375f, 0.10625f, 52.0f},
    {"azure", "ArZoom.PresentationCursor.Preset.Azure",
     BuiltinCursorVisual::Azure, 28, 0.240f, 0.19375f, 0.10625f, 52.0f},
    {"orchid", "ArZoom.PresentationCursor.Preset.Orchid",
     BuiltinCursorVisual::Orchid, 28, 0.270f, 0.19375f, 0.10625f, 52.0f},
}};

inline float cursor_tactile_smoothstep(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/*
 * Positive values compress the cursor around its click hotspot. Negative
 * values deliberately overshoot past the neutral geometry for a very small
 * spring rebound. This is deterministic and has no velocity/state accumulation,
 * so repeated clicks simply restart the gesture without ringing forever.
 */
inline float cursor_tactile_press_curve(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);

    /* 0-18%: extremely fast physical press. Ease-out cubic gives immediate
     * motion on the very first rendered frame while avoiding a hard step. */
    if (t < 0.18f) {
        const float u = t / 0.18f;
        const float inv = 1.0f - u;
        return 1.0f - inv * inv * inv;
    }

    /* 18-42%: release through neutral into a restrained 18% rebound. */
    if (t < 0.42f) {
        const float u = cursor_tactile_smoothstep((t - 0.18f) / 0.24f);
        return 1.0f + (-0.18f - 1.0f) * u;
    }

    /* 42-68%: one tiny counter-bounce. */
    if (t < 0.68f) {
        const float u = cursor_tactile_smoothstep((t - 0.42f) / 0.26f);
        return -0.18f + (0.065f + 0.18f) * u;
    }

    /* 68-100%: settle exactly to the idle geometry. */
    const float u = cursor_tactile_smoothstep((t - 0.68f) / 0.32f);
    return 0.065f * (1.0f - u);
}

inline float cursor_tactile_press(size_t frame_index, size_t frame_count)
{
    if (frame_index == 0 || frame_count <= 2)
        return 0.0f;
    const float t = static_cast<float>(frame_index - 1) /
                    static_cast<float>(frame_count - 2);
    return cursor_tactile_press_curve(t);
}

inline const CursorPreset *find_cursor_preset(const char *id)
{
    if (!id || !*id)
        return nullptr;
    for (const auto &preset : kCursorPresets) {
        if (std::strcmp(id, preset.id) == 0)
            return &preset;
    }
    return nullptr;
}

inline bool cursor_style_is_custom(const char *id)
{
    return id && std::strcmp(id, kCursorStyleCustom) == 0;
}

inline bool cursor_style_is_off(const char *id)
{
    return !id || !*id || std::strcmp(id, kCursorStyleOff) == 0;
}

inline bool cursor_preset_is_valid(const CursorPreset &preset)
{
    return preset.id && *preset.id && preset.locale_key &&
           preset.frame_count > 1 && preset.frame_count <= 64 &&
           preset.play_seconds >= 0.18f && preset.play_seconds <= 0.35f &&
           preset.hotspot_x >= 0.0f && preset.hotspot_x <= 1.0f &&
           preset.hotspot_y >= 0.0f && preset.hotspot_y <= 1.0f &&
           preset.recommended_size_px >= 24.0f &&
           preset.recommended_size_px <= 96.0f;
}

} // namespace arzoom
