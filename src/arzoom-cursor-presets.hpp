#pragma once

#include <array>
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

inline constexpr std::array<CursorPreset, 4> kCursorPresets{{
    {"prism", "ArZoom.PresentationCursor.Preset.Prism",
     BuiltinCursorVisual::Prism, 28, 1.107f, 0.19375f, 0.10625f, 52.0f},
    {"outline", "ArZoom.PresentationCursor.Preset.Outline",
     BuiltinCursorVisual::Outline, 28, 1.107f, 0.19375f, 0.10625f, 52.0f},
    {"azure", "ArZoom.PresentationCursor.Preset.Azure",
     BuiltinCursorVisual::Azure, 28, 1.107f, 0.19375f, 0.10625f, 52.0f},
    {"orchid", "ArZoom.PresentationCursor.Preset.Orchid",
     BuiltinCursorVisual::Orchid, 28, 1.107f, 0.19375f, 0.10625f, 52.0f},
}};

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
           preset.play_seconds >= 0.18f && preset.play_seconds <= 1.50f &&
           preset.hotspot_x >= 0.0f && preset.hotspot_x <= 1.0f &&
           preset.hotspot_y >= 0.0f && preset.hotspot_y <= 1.0f &&
           preset.recommended_size_px >= 24.0f &&
           preset.recommended_size_px <= 96.0f;
}

} // namespace arzoom
