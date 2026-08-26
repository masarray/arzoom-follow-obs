#include "arzoom-filter-v22.cpp"
#include "arzoom-cinematic-spotlight.hpp"

#include <atomic>
#include <cmath>
#include <new>

#define SETTING_SPOTLIGHT_LINK_ZOOM "spotlight_link_to_zoom"
#define SETTING_SPOTLIGHT_CINEMATIC_SPEED "spotlight_cinematic_speed"
#define SETTING_P5_CINEMATIC_DEFAULTS_V1 "p5_cinematic_defaults_v1"

#define CINEMATIC_SPEED_SMOOTH "smooth"
#define CINEMATIC_SPEED_BALANCED "balanced"
#define CINEMATIC_SPEED_SNAPPY "snappy"

namespace {

struct Phase23Filter {
    Phase51Filter *phase51 = nullptr;

    std::atomic<bool> link_to_zoom{true};
    std::atomic<int> cinematic_speed{
        static_cast<int>(arzoom::CinematicFocusSpeed::Balanced)};
    std::atomic<bool> reset_requested{false};

    /* video_tick-owned choreography */
    arzoom::CinematicSpotlightState cinematic{};
    bool cinematic_render_active = false;
    float cinematic_render_scale = 1.0f;
    float cinematic_render_dim_mix = 1.0f;

    /* P5.5 shader extension. These two uniforms are presentation-only and are
     * initialized on every processed Draw, just like the evidence-backed v21
     * neutral ABI contract. */
    gs_eparam_t *cinematic_scale_param = nullptr;
    gs_eparam_t *cinematic_dim_mix_param = nullptr;
    bool cinematic_shader_ready = false;
};

arzoom::CinematicFocusSpeed parse_cinematic_speed(const char *value)
{
    if (value && std::strcmp(value, CINEMATIC_SPEED_SMOOTH) == 0)
        return arzoom::CinematicFocusSpeed::Smooth;
    if (value && std::strcmp(value, CINEMATIC_SPEED_SNAPPY) == 0)
        return arzoom::CinematicFocusSpeed::Snappy;
    return arzoom::CinematicFocusSpeed::Balanced;
}

bool phase23_manual_spotlight_requested(Phase51Filter *filter)
{
    return filter &&
           (filter->latched_active.load(std::memory_order_acquire) ||
            filter->hold_active.load(std::memory_order_acquire) ||
            filter->gui_peek_remaining > 0.0f);
}

bool phase23_update_auto_center(Phase51Filter *filter, float dt)
{
    if (!filter || !filter->phase5)
        return false;

    ArZoomFilter *phase1 = phase51_phase1(filter);
    if (!phase1)
        return false;

    const auto mode = static_cast<arzoom::SpotlightMode>(
        filter->phase5->spotlight_mode.load(std::memory_order_acquire));

    if (mode == arzoom::SpotlightMode::Click) {
        capture_phase51_click(filter);
        if (!filter->click_anchor_valid)
            return false;
        move_phase51_center(
            filter,
            content_to_live_output(phase1, filter->click_anchor_content),
            dt, 0.0f);
        return filter->center_valid;
    }

    arzoom::Vec2 pointer_output;
    if (!phase51_pointer_output(filter, pointer_output))
        return filter->center_valid;

    if (mode == arzoom::SpotlightMode::Cursor) {
        move_phase51_center(filter, pointer_output, dt, 0.055f);
        return filter->center_valid;
    }

    /* Smart Focus remains a read-only consumer of the same accepted context
     * threshold as the camera. It never writes requested_zoom or camera state. */
    if (!filter->center_valid) {
        move_phase51_center(filter, pointer_output, dt, 0.0f);
    } else {
        const float distance_output = arzoom::length(
            arzoom::sub(pointer_output, filter->center_output));
        const float wake = arzoom::scene_context_wake_half(
            phase1->safe_zone.load(std::memory_order_acquire),
            std::max(phase1->current_zoom, 1.0f));
        if (camera_is_semantically_moving(phase1) || distance_output >= wake)
            move_phase51_center(filter, pointer_output, dt, 0.125f);
    }
    return filter->center_valid;
}

void phase23_publish_cinematic_visual(Phase23Filter *wrapper,
                                      float focus_mix)
{
    Phase51Filter *filter = wrapper ? wrapper->phase51 : nullptr;
    ArZoomFilter *phase1 = phase51_phase1(filter);
    if (!wrapper || !filter || !filter->phase5 || !phase1)
        return;

    obs_source_t *target = obs_filter_get_target(phase1->context);
    const float width = static_cast<float>(
        target ? std::max(obs_source_get_width(target), 1u) : 1u);
    const float height = static_cast<float>(
        target ? std::max(obs_source_get_height(target), 1u) : 1u);

    const arzoom::SpotlightVec2 base = arzoom::spotlight_half_size_px(
        arzoom::SpotlightSize::Balanced, width, height);
    const float base_radius = std::max(1.0f, std::min(base.x, base.y));
    const float feather = filter->phase5->spotlight_feather_px.load(
        std::memory_order_acquire);
    const float full_radius = arzoom::cinematic_full_radius_px(
        width, height,
        filter->center_output.x, filter->center_output.y,
        feather);
    const float full_area_percent = arzoom::cinematic_full_area_percent(
        base_radius, full_radius);
    const float working_area_percent = std::clamp(
        filter->area_scale_percent.load(std::memory_order_acquire),
        50.0f, 200.0f);

    const float visual_area_percent =
        full_area_percent +
        (working_area_percent - full_area_percent) *
            std::clamp(focus_mix, 0.0f, 1.0f);

    wrapper->cinematic_render_scale = std::clamp(
        visual_area_percent / std::max(working_area_percent, 1.0f),
        1.0f, 64.0f);
    wrapper->cinematic_render_dim_mix =
        arzoom::cinematic_dim_mix(focus_mix);
    wrapper->cinematic_render_active = true;
}

void phase23_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    Phase51Filter *filter = wrapper->phase51;
    phase51_tick(filter, seconds);

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    ArZoomFilter *phase1 = phase51_phase1(filter);
    if (!phase1 || !filter->phase5)
        return;

    if (wrapper->reset_requested.exchange(false, std::memory_order_acq_rel)) {
        wrapper->cinematic.reset();
        wrapper->cinematic_render_active = false;
        wrapper->cinematic_render_scale = 1.0f;
        wrapper->cinematic_render_dim_mix = 1.0f;
    }

    const bool master =
        filter->phase5->spotlight_enabled.load(std::memory_order_acquire);
    const bool link = wrapper->link_to_zoom.load(std::memory_order_acquire);
    const bool globally_enabled =
        phase1->enabled.load(std::memory_order_acquire);
    const bool manual = phase23_manual_spotlight_requested(filter);

    if (!master || !link || !globally_enabled ||
        !filter->phase5->shader_ready || !filter->extension_shader_ready ||
        !wrapper->cinematic_shader_ready) {
        wrapper->cinematic.reset();
        wrapper->cinematic_render_active = false;
        return;
    }

    const bool zoom_requested =
        phase1->requested_zoom.load(std::memory_order_acquire);

    /* Acquire the focus while the aperture is still visually full-frame.  If
     * Click mode has no anchor, or first-use pointer mapping is unavailable,
     * do not guess: keep the aperture full until a proven target exists. */
    bool focus_ready = filter->center_valid;
    if (zoom_requested || wrapper->cinematic.visually_active())
        focus_ready = phase23_update_auto_center(filter, dt);

    const bool auto_focus_requested = zoom_requested && focus_ready;
    const auto speed = static_cast<arzoom::CinematicFocusSpeed>(
        wrapper->cinematic_speed.load(std::memory_order_acquire));
    wrapper->cinematic.set_target(auto_focus_requested, speed);
    wrapper->cinematic.step(dt);

    /* Manual Toggle/Hold/Peek keeps its existing immediate semantics and wins
     * visually over auto choreography. The auto state can continue bounded in
     * the background so releasing manual intent during an active Zoom is calm. */
    if (manual) {
        wrapper->cinematic_render_active = false;
        return;
    }

    if (wrapper->cinematic.visually_active() || auto_focus_requested) {
        phase23_publish_cinematic_visual(wrapper, wrapper->cinematic.value);
        filter->phase5->runtime_active.store(true,
                                             std::memory_order_release);
        return;
    }

    wrapper->cinematic_render_active = false;
    /* phase51_tick already published the correct manual/off state. With no
     * manual intent and the opening complete, return to true pass-through. */
    filter->phase5->runtime_active.store(false, std::memory_order_release);
}

bool phase23_migrate_defaults(obs_data_t *settings)
{
    if (!settings ||
        obs_data_has_user_value(settings, SETTING_P5_CINEMATIC_DEFAULTS_V1)) {
        return false;
    }

    /* P5 is still a WIP branch. Apply the newly accepted product defaults once
     * so existing trial filters can be retested without hand-editing every
     * field. Master Spotlight enable is intentionally NOT changed here. */
    obs_data_set_string(settings, SETTING_SPOTLIGHT_MODE,
                        SPOTLIGHT_MODE_CURSOR);
    obs_data_set_string(settings, SETTING_SPOTLIGHT_SHAPE,
                        SPOTLIGHT_SHAPE_CIRCLE);
    obs_data_set_int(settings, SETTING_SPOTLIGHT_AREA_SCALE, 170);
    obs_data_set_int(settings, SETTING_SPOTLIGHT_DIM, 35);
    obs_data_set_int(settings, SETTING_SPOTLIGHT_FEATHER, 40);
    obs_data_set_string(settings, SETTING_PRESENTATION_CURSOR_STYLE,
                        "classic_hand");
    obs_data_set_bool(settings, SETTING_SPOTLIGHT_LINK_ZOOM, true);
    obs_data_set_string(settings, SETTING_SPOTLIGHT_CINEMATIC_SPEED,
                        CINEMATIC_SPEED_BALANCED);
    obs_data_set_bool(settings, SETTING_P5_CINEMATIC_DEFAULTS_V1, true);

    blog(LOG_INFO,
         "[ArZoom] P5 cinematic defaults applied: cursor focus, 170%% area, "
         "35%% dim, 40 px softness, Classic Hand");
    return true;
}

void phase23_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    phase23_migrate_defaults(settings);
    phase52_update(wrapper->phase51, settings);

    wrapper->link_to_zoom.store(
        obs_data_get_bool(settings, SETTING_SPOTLIGHT_LINK_ZOOM),
        std::memory_order_release);
    wrapper->cinematic_speed.store(
        static_cast<int>(parse_cinematic_speed(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_CINEMATIC_SPEED))),
        std::memory_order_release);
    wrapper->reset_requested.store(true, std::memory_order_release);
}

void phase23_defaults(obs_data_t *settings)
{
    phase51_defaults(settings);

    /* Spotlight remains opt-in at the master level. Once enabled, these are the
     * new beginner defaults requested for the cinematic presentation workflow. */
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_MODE,
                                SPOTLIGHT_MODE_CURSOR);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_SHAPE,
                                SPOTLIGHT_SHAPE_CIRCLE);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_AREA_SCALE, 170);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_DIM, 35);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_FEATHER, 40);
    obs_data_set_default_string(settings, SETTING_PRESENTATION_CURSOR_STYLE,
                                "classic_hand");
    obs_data_set_default_bool(settings, SETTING_SPOTLIGHT_LINK_ZOOM, true);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_CINEMATIC_SPEED,
                                CINEMATIC_SPEED_BALANCED);
    obs_data_set_default_bool(settings, SETTING_P5_CINEMATIC_DEFAULTS_V1,
                              false);
}

obs_properties_t *phase23_properties(void *data)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    Phase51Filter *filter = wrapper ? wrapper->phase51 : nullptr;
    obs_properties_t *props = phase41_properties(
        filter && filter->phase5 ? filter->phase5->phase41 : nullptr);

    obs_properties_t *spotlight = obs_properties_create();

    obs_property_t *info = obs_properties_add_text(
        spotlight, "spotlight_info",
        obs_module_text("ArZoom.Spotlight.Info.Cinematic"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    obs_properties_add_bool(
        spotlight, SETTING_SPOTLIGHT_ENABLED,
        obs_module_text("ArZoom.Spotlight.Enabled.P51"));
    obs_properties_add_bool(
        spotlight, SETTING_SPOTLIGHT_LINK_ZOOM,
        obs_module_text("ArZoom.Spotlight.CinematicZoom"));

    obs_property_t *speed = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_CINEMATIC_SPEED,
        obs_module_text("ArZoom.Spotlight.CinematicSpeed"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        speed, obs_module_text("ArZoom.Spotlight.CinematicSpeed.Smooth"),
        CINEMATIC_SPEED_SMOOTH);
    obs_property_list_add_string(
        speed, obs_module_text("ArZoom.Spotlight.CinematicSpeed.Balanced"),
        CINEMATIC_SPEED_BALANCED);
    obs_property_list_add_string(
        speed, obs_module_text("ArZoom.Spotlight.CinematicSpeed.Snappy"),
        CINEMATIC_SPEED_SNAPPY);

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
        obs_module_text("ArZoom.Spotlight.Feather"), 24, 180, 2);
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

void phase23_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    Phase51Filter *filter = wrapper ? wrapper->phase51 : nullptr;
    if (!wrapper || !filter || !filter->phase5)
        return;

    const bool presentation_required = phase21_presentation_required(filter);
    if (presentation_required) {
        const bool base_abi_ready = phase21_prime_neutral_spotlight_abi(filter);
        if (!base_abi_ready || !wrapper->cinematic_shader_ready) {
            static std::atomic<bool> warned{false};
            if (!warned.exchange(true, std::memory_order_acq_rel)) {
                blog(LOG_ERROR,
                     "[ArZoom][P0TRACE] Cinematic Spotlight shader ABI incomplete; "
                     "using safe source pass-through");
            }
            ArZoomFilter *phase1 = phase51_phase1(filter);
            if (phase1 && phase1->context)
                obs_source_skip_video_filter(phase1->context);
            return;
        }

        /* New P5.5 uniforms are always initialized on every processed draw.
         * This preserves the exact lesson from the v21 D3D11 ABI fix. */
        gs_effect_set_float(wrapper->cinematic_scale_param, 1.0f);
        gs_effect_set_float(wrapper->cinematic_dim_mix_param, 1.0f);
        if (wrapper->cinematic_render_active) {
            gs_effect_set_float(wrapper->cinematic_scale_param,
                                wrapper->cinematic_render_scale);
            gs_effect_set_float(wrapper->cinematic_dim_mix_param,
                                wrapper->cinematic_render_dim_mix);
        }
    }

    /* phase18 remains the single owner of camera/click/cursor rendering. */
    phase18_render(filter, effect);
}

void phase23_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;
    wrapper->cinematic.reset();
    wrapper->cinematic_render_active = false;
    phase51_deactivate(wrapper->phase51);
}

void phase23_destroy(void *data)
{
    auto *wrapper = static_cast<Phase23Filter *>(data);
    if (!wrapper)
        return;
    phase51_destroy(wrapper->phase51);
    delete wrapper;
}

void *phase23_create(obs_data_t *settings, obs_source_t *context)
{
    phase23_migrate_defaults(settings);
    auto *phase51 = static_cast<Phase51Filter *>(
        phase52_create(settings, context));
    if (!phase51)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase23Filter();
    if (!wrapper) {
        phase51_destroy(phase51);
        return nullptr;
    }
    wrapper->phase51 = phase51;

    ArZoomFilter *phase1 = phase51_phase1(phase51);
    if (phase1 && phase1->effect) {
        wrapper->cinematic_scale_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_cinematic_scale");
        wrapper->cinematic_dim_mix_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_cinematic_dim_mix");
        wrapper->cinematic_shader_ready =
            wrapper->cinematic_scale_param &&
            wrapper->cinematic_dim_mix_param;
    }

    wrapper->link_to_zoom.store(
        obs_data_get_bool(settings, SETTING_SPOTLIGHT_LINK_ZOOM),
        std::memory_order_release);
    wrapper->cinematic_speed.store(
        static_cast<int>(parse_cinematic_speed(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_CINEMATIC_SPEED))),
        std::memory_order_release);
    wrapper->cinematic.reset();

    if (!wrapper->cinematic_shader_ready) {
        blog(LOG_ERROR,
             "[ArZoom] P5 cinematic shader extension unavailable; "
             "Zoom-linked Spotlight remains fail-safe disabled.");
    } else {
        blog(LOG_INFO,
             "[ArZoom] P5 cinematic Zoom-linked Spotlight ready");
    }
    return wrapper;
}

struct Phase23SourceInfoOverride {
    Phase23SourceInfoOverride()
    {
        arzoom_filter_info.create = phase23_create;
        arzoom_filter_info.destroy = phase23_destroy;
        arzoom_filter_info.video_tick = phase23_tick;
        arzoom_filter_info.video_render = phase23_render;
        arzoom_filter_info.update = phase23_update;
        arzoom_filter_info.get_properties = phase23_properties;
        arzoom_filter_info.get_defaults = phase23_defaults;
        arzoom_filter_info.deactivate = phase23_deactivate;
    }
};

Phase23SourceInfoOverride phase23_source_info_override;

} // namespace
