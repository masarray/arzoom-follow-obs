#include "arzoom-click-visual.hpp"

#include <array>
#include <atomic>
#include <new>

#include <graphics/vec4.h>

/*
 * Phase 2 deliberately layers click visualization around the frozen Phase 1
 * runtime instead of editing the Smart Zone camera implementation. The Phase 1
 * implementation is compiled once inside this translation unit; the callback
 * table is then replaced with thin wrappers that own only click-visual state.
 */
#include "arzoom-filter-v2.cpp"

#define SETTING_CLICK_VISUAL "click_visual_enabled"

namespace {

struct Phase2Filter {
    ArZoomFilter *phase1 = nullptr;
    arzoom::ClickVisualState clicks;
    std::atomic<bool> click_visual_enabled{true};

    /* P3.5 may reuse the exact same Windows click sample for cursor animation
     * even when the visual rings are disabled. Default false preserves the
     * frozen Phase 2 behavior bit-for-bit. */
    std::atomic<bool> click_capture_for_cursor{false};

    bool left_down = false;
    bool right_down = false;
    bool middle_down = false;

    std::array<gs_eparam_t *, arzoom::ClickVisualState::kSlotCount>
        click_params{};
    gs_eparam_t *viewport_size_param = nullptr;
    bool click_shader_ready = false;
};

#ifdef _WIN32
bool mouse_button_pressed(int virtual_key, bool &was_down)
{
    const SHORT state = GetAsyncKeyState(virtual_key);
    const bool down = (state & 0x8000) != 0;
    const bool pressed_since_sample = (state & 0x0001) != 0;
    const bool pressed = (down && !was_down) || pressed_since_sample;
    was_down = down;
    return pressed;
}
#else
bool mouse_button_pressed(int, bool &was_down)
{
    was_down = false;
    return false;
}
#endif

void clear_button_edges(Phase2Filter *filter)
{
    filter->left_down = false;
    filter->right_down = false;
    filter->middle_down = false;
}

void capture_clicks(Phase2Filter *filter, float dt)
{
    filter->clicks.advance(dt);

    const bool capture_needed =
        filter->click_visual_enabled.load(std::memory_order_acquire) ||
        filter->click_capture_for_cursor.load(std::memory_order_acquire);
    if (!capture_needed ||
        !filter->phase1->enabled.load(std::memory_order_acquire)) {
        filter->clicks.clear();
        clear_button_edges(filter);
        return;
    }

#ifdef _WIN32
    const bool left = mouse_button_pressed(VK_LBUTTON, filter->left_down);
    const bool right = mouse_button_pressed(VK_RBUTTON, filter->right_down);
    const bool middle = mouse_button_pressed(VK_MBUTTON, filter->middle_down);
#else
    const bool left = false;
    const bool right = false;
    const bool middle = false;
#endif

    if (!left && !right && !middle)
        return;

    if (!filter->phase1->monitor_valid)
        return;

    long cursor_x = 0;
    long cursor_y = 0;
    if (!get_cursor_position(cursor_x, cursor_y) ||
        !cursor_in_monitor(filter->phase1->monitor, cursor_x, cursor_y)) {
        return;
    }

    const arzoom::Vec2 position = cursor_normalized(
        filter->phase1->monitor, cursor_x, cursor_y);
    if (left)
        filter->clicks.push(arzoom::ClickType::Left, position);
    if (right)
        filter->clicks.push(arzoom::ClickType::Right, position);
    if (middle)
        filter->clicks.push(arzoom::ClickType::Middle, position);
}

void phase2_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    update(filter->phase1, settings);
    filter->click_visual_enabled.store(
        obs_data_get_bool(settings, SETTING_CLICK_VISUAL),
        std::memory_order_release);
    if (!filter->click_visual_enabled.load(std::memory_order_acquire) &&
        !filter->click_capture_for_cursor.load(std::memory_order_acquire)) {
        filter->clicks.clear();
    }
}

void phase2_defaults(obs_data_t *settings)
{
    defaults(settings);
    obs_data_set_default_bool(settings, SETTING_CLICK_VISUAL, true);
}

obs_properties_t *phase2_properties(void *data)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    obs_properties_t *props = properties(filter ? filter->phase1 : nullptr);

    obs_properties_t *click_group = obs_properties_create();
    obs_properties_add_bool(
        click_group, SETTING_CLICK_VISUAL,
        obs_module_text("ArZoom.ClickVisual.Enabled"));
    obs_property_t *info = obs_properties_add_text(
        click_group, "click_visual_info",
        obs_module_text("ArZoom.ClickVisual.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);
    obs_properties_add_group(
        props, "click_visual_group",
        obs_module_text("ArZoom.ClickVisual.Group"),
        OBS_GROUP_NORMAL, click_group);
    return props;
}

void phase2_deactivate(void *data)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    deactivate(filter->phase1);
    filter->clicks.clear();
    clear_button_edges(filter);
}

void phase2_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    tick(filter->phase1, seconds);
    capture_clicks(filter, std::clamp(seconds, 0.0f, 0.10f));
}

void set_click_uniform(gs_eparam_t *param, const arzoom::ClickEvent &event)
{
    if (!param)
        return;
    vec4 packed;
    if (event.active()) {
        vec4_set(&packed,
                 event.content_position.x,
                 event.content_position.y,
                 event.age_seconds,
                 static_cast<float>(event.type));
    } else {
        vec4_set(&packed, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    gs_effect_set_vec4(param, &packed);
}

void phase2_render(void *data, gs_effect_t *)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    ArZoomFilter *phase1 = filter->phase1;

    const bool camera_active =
        phase1->current_zoom > 1.0005f ||
        !arzoom::nearly_equal(phase1->current_center,
                              {0.5f, 0.5f}, 0.0005f);
    const bool click_active =
        filter->click_visual_enabled.load(std::memory_order_acquire) &&
        filter->click_shader_ready && filter->clicks.has_active();

    if (!phase1->effect_ready || !phase1->effect ||
        !phase1->enabled.load(std::memory_order_acquire) ||
        (!camera_active && !click_active)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    if (!obs_source_process_filter_begin(
            phase1->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    gs_effect_set_float(phase1->zoom_param,
                        std::max(phase1->current_zoom, 1.0f));
    vec2 center;
    vec2_set(&center, phase1->current_center.x, phase1->current_center.y);
    gs_effect_set_vec2(phase1->center_param, &center);

    obs_source_t *target = obs_filter_get_target(phase1->context);
    const float width = static_cast<float>(
        target ? std::max(obs_source_get_width(target), 1u) : 1u);
    const float height = static_cast<float>(
        target ? std::max(obs_source_get_height(target), 1u) : 1u);
    if (filter->viewport_size_param) {
        vec2 viewport;
        vec2_set(&viewport, width, height);
        gs_effect_set_vec2(filter->viewport_size_param, &viewport);
    }

    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i)
        set_click_uniform(filter->click_params[i], filter->clicks.slot(i));

    obs_source_process_filter_end(
        phase1->context, phase1->effect, 0, 0);
}

void phase2_destroy(void *data)
{
    auto *filter = static_cast<Phase2Filter *>(data);
    if (!filter)
        return;
    destroy(filter->phase1);
    delete filter;
}

void *phase2_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase1 = static_cast<ArZoomFilter *>(create(settings, context));
    if (!phase1)
        return nullptr;

    auto *filter = new (std::nothrow) Phase2Filter();
    if (!filter) {
        destroy(phase1);
        return nullptr;
    }
    filter->phase1 = phase1;

    if (phase1->effect) {
        static const char *names[] = {
            "click_event0", "click_event1", "click_event2", "click_event3"};
        bool all_click_params = true;
        for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i) {
            filter->click_params[i] = gs_effect_get_param_by_name(
                phase1->effect, names[i]);
            all_click_params = all_click_params && filter->click_params[i];
        }
        filter->viewport_size_param = gs_effect_get_param_by_name(
            phase1->effect, "viewport_size");
        filter->click_shader_ready =
            all_click_params && filter->viewport_size_param;
    }

    if (!filter->click_shader_ready) {
        blog(LOG_WARNING,
             "[ArZoom] Click visualization shader parameters are unavailable; "
             "Smart Camera remains functional without click overlay.");
    } else {
        blog(LOG_INFO,
             "[ArZoom] Phase 2 one-pass GPU click visualization ready");
    }

    phase2_update(filter, settings);
    return filter;
}

/* This runs after the initializer inside arzoom-filter-v2.cpp in the same
 * translation unit, replacing only callbacks that need the Phase 2 wrapper. */
struct Phase2SourceInfoOverride {
    Phase2SourceInfoOverride()
    {
        arzoom_filter_info.create = phase2_create;
        arzoom_filter_info.destroy = phase2_destroy;
        arzoom_filter_info.video_tick = phase2_tick;
        arzoom_filter_info.video_render = phase2_render;
        arzoom_filter_info.update = phase2_update;
        arzoom_filter_info.get_properties = phase2_properties;
        arzoom_filter_info.get_defaults = phase2_defaults;
        arzoom_filter_info.deactivate = phase2_deactivate;
    }
};

Phase2SourceInfoOverride phase2_source_info_override;

} // namespace