#pragma once

namespace arzoom {

/* Native OBS settings presentation only. No source state or camera/renderer
 * semantics depend on this policy. The existing settings keys remain stable
 * across upgrades and are never reset when a panel is hidden. */
struct CompactPropertiesPolicy {
    bool advanced = false;
    bool spotlight_enabled = false;
    bool cinematic_with_zoom = false;

    constexpr bool show_spotlight_primary() const noexcept
    {
        return spotlight_enabled;
    }
    constexpr bool show_spotlight_advanced() const noexcept
    {
        return advanced && spotlight_enabled;
    }
    constexpr bool show_cinematic_speed() const noexcept
    {
        return advanced && spotlight_enabled && cinematic_with_zoom;
    }
    constexpr bool show_camera_advanced() const noexcept
    {
        return advanced;
    }
};

} // namespace arzoom
