#include "arzoom-filter-v21.cpp"

/*
 * P5 UX / v22 — zero-refresh Spotlight controls
 * =============================================
 *
 * Direct OBS feedback showed the Properties dialog briefly reconstructing its
 * slider widgets whenever the GUI Toggle Spotlight button was pressed.  The
 * cause is deterministic: phase51_toggle_button() returned true, which asks OBS
 * to refresh/rebuild the whole property sheet so a dynamic ON/OFF text label
 * can change.
 *
 * Runtime presenter actions must not rebuild configuration UI.  v22 therefore
 * keeps all render/state behavior from v21 and replaces only get_properties:
 * - Toggle Spotlight mutates atomic runtime intent and returns false;
 * - Peek remains zero-refresh;
 * - the rebuild-dependent runtime status text is removed;
 * - configuration sliders/combos remain ordinary OBS properties.
 */
namespace {

bool phase22_toggle_button(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!phase51_available(filter))
        return false;

    const bool current =
        filter->latched_active.load(std::memory_order_acquire);
    filter->latched_active.store(!current, std::memory_order_release);
    filter->transition_generation.fetch_add(1, std::memory_order_acq_rel);

    /* IMPORTANT: false means do not rebuild the OBS Properties sheet. */
    return false;
}

obs_properties_t *phase22_properties(void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    obs_properties_t *props = phase41_properties(
        filter && filter->phase5 ? filter->phase5->phase41 : nullptr);

    obs_properties_t *spotlight = obs_properties_create();

    obs_property_t *info = obs_properties_add_text(
        spotlight, "spotlight_info",
        obs_module_text("ArZoom.Spotlight.Info.P51"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    obs_properties_add_bool(
        spotlight, SETTING_SPOTLIGHT_ENABLED,
        obs_module_text("ArZoom.Spotlight.Enabled.P51"));

    obs_property_t *mode = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_MODE,
        obs_module_text("ArZoom.Spotlight.Mode"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Smart"),
        SPOTLIGHT_MODE_SMART);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Cursor"),
        SPOTLIGHT_MODE_CURSOR);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Click"),
        SPOTLIGHT_MODE_CLICK);

    obs_property_t *shape = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_SHAPE,
        obs_module_text("ArZoom.Spotlight.Shape"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.Circle"),
        SPOTLIGHT_SHAPE_CIRCLE);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.Ellipse"),
        SPOTLIGHT_SHAPE_ELLIPSE);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.RoundedRect"),
        SPOTLIGHT_SHAPE_ROUNDED_RECT);

    obs_property_t *area = obs_properties_add_int_slider(
        spotlight, SETTING_SPOTLIGHT_AREA_SCALE,
        obs_module_text("ArZoom.Spotlight.AreaSize"), 50, 200, 5);
    obs_property_int_set_suffix(area, " %");

    obs_property_t *dim = obs_properties_add_int_slider(
        spotlight, SETTING_SPOTLIGHT_DIM,
        obs_module_text("ArZoom.Spotlight.Dim"), 15, 60, 1);
    obs_property_int_set_suffix(dim, " %");

    if (filter) {
        obs_properties_add_button2(
            spotlight, "spotlight_toggle_now",
            obs_module_text("ArZoom.Spotlight.ToggleButton"),
            phase22_toggle_button, filter);
        obs_properties_add_button2(
            spotlight, "spotlight_peek_now",
            obs_module_text("ArZoom.Spotlight.PeekButton"),
            phase51_peek_button, filter);
    }

    obs_properties_t *advanced = obs_properties_create();
    obs_property_t *feather = obs_properties_add_int_slider(
        advanced, SETTING_SPOTLIGHT_FEATHER,
        obs_module_text("ArZoom.Spotlight.Feather"), 30, 180, 2);
    obs_property_int_set_suffix(feather, " px");

    obs_property_t *hotkey_info = obs_properties_add_text(
        advanced, "spotlight_hotkey_info",
        obs_module_text("ArZoom.Spotlight.HotkeyInfo"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(hotkey_info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(hotkey_info, true);

    obs_properties_add_button(
        advanced, "spotlight_open_hotkeys",
        obs_module_text("ArZoom.OpenHotkeys"), open_hotkeys_clicked);

    obs_properties_add_group(
        spotlight, "spotlight_advanced",
        obs_module_text("ArZoom.Advanced"), OBS_GROUP_NORMAL, advanced);

    obs_properties_add_group(
        props, "spotlight_group", obs_module_text("ArZoom.Spotlight.Group"),
        OBS_GROUP_NORMAL, spotlight);
    return props;
}

struct Phase22SourceInfoOverride {
    Phase22SourceInfoOverride()
    {
        arzoom_filter_info.get_properties = phase22_properties;
    }
};

Phase22SourceInfoOverride phase22_source_info_override;

} // namespace
