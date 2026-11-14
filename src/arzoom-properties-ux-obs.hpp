#pragma once

#include "arzoom-properties-ux-policy.hpp"
#include <obs-module.h>

/*
 * ArZoom compact native OBS Properties composition
 * ------------------------------------------------
 * The historic P2/P3/P4/P5 property constructors remain the single owners
 * of setting bindings, defaults and runtime actions. This final-stage adapter
 * only adjusts visibility, accessibility copy and progressive disclosure.
 * NO runtime callback, graphics path, camera state or setting key is replaced.
 */
namespace arzoom::ux {

constexpr const char *kAdvancedSettingsKey = "ui_show_advanced";

inline void visible(obs_properties_t *props, const char *name,
                    bool enabled) noexcept
{
    if (!props)
        return;
    if (obs_property_t *property = obs_properties_get(props, name))
        obs_property_set_visible(property, enabled);
}

inline void description(obs_properties_t *props, const char *name,
                        const char *translation_key) noexcept
{
    if (!props)
        return;
    if (obs_property_t *property = obs_properties_get(props, name))
        obs_property_set_long_description(
            property, obs_module_text(translation_key));
}

inline obs_properties_t *subgroup(obs_properties_t *props,
                                 const char *name) noexcept
{
    if (!props)
        return nullptr;
    obs_property_t *property = obs_properties_get(props, name);
    return property && obs_property_get_type(property) == OBS_PROPERTY_GROUP
               ? obs_property_group_content(property)
               : nullptr;
}

inline CompactPropertiesPolicy policy(obs_data_t *settings) noexcept
{
    if (!settings)
        return {};
    return {
        obs_data_get_bool(settings, kAdvancedSettingsKey),
        obs_data_get_bool(settings, "spotlight_enabled"),
        obs_data_get_bool(settings, "spotlight_link_to_zoom")
    };
}

inline void apply(obs_properties_t *root,
                  const CompactPropertiesPolicy &state) noexcept
{
    if (!root)
        return;

    /* One canonical hotkey entry point, no duplicate documentation blocks.
     * Save Bindings is rare and remains discoverable under Advanced. */
    visible(root, "advanced", state.show_camera_advanced());
    visible(root, "save_hotkey", state.show_camera_advanced());
    visible(root, "hotkey_status", false);
    visible(root, "presenter_controls", false);

    if (obs_properties_t *clicks = subgroup(root, "click_visual_group"))
        visible(clicks, "click_visual_info", false);

    if (obs_properties_t *cursor =
            subgroup(root, "presentation_cursor_group")) {
        visible(cursor, "presentation_cursor_preset_info", false);
        /* Keep contextual warning/error messages and Custom asset controls:
         * they are actionable, not noisy onboarding paragraphs. */
    }

    if (obs_properties_t *spotlight = subgroup(root, "spotlight_group")) {
        visible(spotlight, "spotlight_info", false);
        visible(spotlight, "spotlight_cinematic_speed",
                state.show_cinematic_speed());
        visible(spotlight, "spotlight_shape",
                state.show_spotlight_advanced());

        /* A disabled spotlight exposes one meaningful control: Enable.
         * No confusing enabled-but-irrelevant dim/shape/mode fields. */
        constexpr const char *kPrimary[] = {
            "spotlight_link_to_zoom", "spotlight_mode",
            "spotlight_area_scale", "spotlight_dim",
            "spotlight_toggle_now", "spotlight_peek_now"
        };
        for (const char *name : kPrimary)
            visible(spotlight, name, state.show_spotlight_primary());

        visible(spotlight, "spotlight_advanced",
                state.show_spotlight_advanced());
        if (obs_properties_t *advanced =
                subgroup(spotlight, "spotlight_advanced")) {
            visible(advanced, "spotlight_hotkey_info", false);
            visible(advanced, "spotlight_open_hotkeys", false);
        }
    }

    if (obs_properties_t *keycaps =
            subgroup(root, "shortcut_keycap_group"))
        visible(keycaps, "shortcut_keycap_info", false);
}

/* In OBS, settings layout callbacks are allowed to refresh the Properties
 * panel. Only configuring advanced visibility or Spotlight options uses this;
 * runtime Toggle/Peek buttons retain their existing no-refresh behavior. */
inline bool modified(obs_properties_t *any, obs_property_t *,
                     obs_data_t *settings)
{
    obs_properties_t *root = any;
    while (root && obs_properties_get_parent(root))
        root = obs_properties_get_parent(root);
    if (!root)
        return false;
    apply(root, policy(settings));
    return true;
}

inline void attach_contextual_help(obs_properties_t *root) noexcept
{
    description(root, "follow_mode", "ArZoom.UX.Help.Follow");
    description(root, "safe_zone", "ArZoom.UX.Help.Range");
    description(root, "open_hotkeys", "ArZoom.UX.Help.Hotkeys");
    description(root, kAdvancedSettingsKey, "ArZoom.UX.Help.Advanced");

    if (obs_properties_t *clicks = subgroup(root, "click_visual_group"))
        description(clicks, "click_visual_enabled",
                    "ArZoom.UX.Help.Clicks");

    if (obs_properties_t *cursor =
            subgroup(root, "presentation_cursor_group"))
        description(cursor, "presentation_cursor_style",
                    "ArZoom.UX.Help.Cursor");

    if (obs_properties_t *spotlight = subgroup(root, "spotlight_group"))
        description(spotlight, "spotlight_enabled",
                    "ArZoom.UX.Help.Spotlight");

    if (obs_properties_t *keycaps =
            subgroup(root, "shortcut_keycap_group"))
        description(keycaps, "shortcut_keycaps_enabled",
                    "ArZoom.UX.Help.Keycaps");
}

} // namespace arzoom::ux
