#include "arzoom-filter-v10.cpp"
#include "arzoom-render-safety.hpp"
#include "arzoom-scene-camera-core.hpp"

#include <graphics/matrix4.h>
#include <graphics/vec3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

/*
 * Phase 4 scene-camera integration
 * --------------------------------
 * A managed Scene Camera is the accepted arzoom_filter attached directly to an
 * OBS scene source. This thin wrapper keeps P1-P3.5 untouched and only replaces
 * monitor/content mapping when the filter target is a scene.
 *
 * Initial mapping is deliberately conservative: one visible top-level Display
 * Capture must fill the scene canvas according to OBS's own scene-item box
 * transform. Anything cropped/scaled/rotated/multiple is treated as ambiguous
 * rather than guessing cursor coordinates.
 */
namespace {

constexpr float kSceneMappingRefreshSeconds = 0.40f;

struct Phase4SceneFilter {
    Phase354Filter *phase354 = nullptr;
    float mapping_refresh_elapsed = 1.0f;
    bool mapping_valid = false;
    bool mapping_warning_logged = false;
    bool nested_cursor_warning_logged = false;

    /* The cursor sampler shares the same one-pass effect as click feedback.
     * Keep a permanent transparent texture available so a fresh-install click
     * never enters the effect with cursor_atlas unbound. */
    gs_texture_t *cursor_fallback_texture = nullptr;
    bool cursor_fallback_bound = false;
};

Phase352Filter *phase352_from_phase4(Phase4SceneFilter *filter)
{
    return filter && filter->phase354 && filter->phase354->phase353
               ? filter->phase354->phase353->phase352
               : nullptr;
}

Phase35Filter *phase35_from_phase4(Phase4SceneFilter *filter)
{
    Phase352Filter *phase352 = phase352_from_phase4(filter);
    return phase352 && phase352->phase351
               ? phase352->phase351->phase35
               : nullptr;
}

ArZoomFilter *phase1_from_phase4(Phase4SceneFilter *filter)
{
    Phase352Filter *phase352 = phase352_from_phase4(filter);
    return phase352 ? phase1_from_352(phase352) : nullptr;
}

gs_texture_t *create_transparent_cursor_fallback()
{
    const uint8_t transparent_pixel[4] = {0, 0, 0, 0};
    const uint8_t *texture_data[1] = {transparent_pixel};

    obs_enter_graphics();
    gs_texture_t *texture =
        gs_texture_create(1, 1, GS_RGBA, 1, texture_data, 0);
    obs_leave_graphics();
    return texture;
}

void prime_cursor_sampler_safety(Phase4SceneFilter *filter)
{
    Phase35Filter *cursor = phase35_from_phase4(filter);
    if (!cursor || !cursor->cursor_shader_ready ||
        !filter->cursor_fallback_texture) {
        return;
    }

    bool atlas_ready = false;
    {
        std::lock_guard<std::mutex> lock(cursor->asset_mutex);
        atlas_ready = cursor->atlas_texture != nullptr &&
                      cursor->frame_width > 0 && cursor->frame_height > 0 &&
                      cursor->frame_count > 0;
    }

    const bool cursor_enabled =
        cursor->cursor_enabled.load(std::memory_order_acquire);
    const bool fallback_required =
        arzoom::cursor_requires_transparent_fallback(
            cursor_enabled,
            cursor->cursor_position_valid,
            cursor->cursor_shader_ready,
            atlas_ready);

    if (!fallback_required) {
        /* phase35_render() will bind the real atlas in this frame.  Clear the
         * latch so the next inactive/not-ready transition rebinds fallback. */
        filter->cursor_fallback_bound = false;
        return;
    }

    if (!filter->cursor_fallback_bound) {
        vec2 center;
        vec2 one;
        vec2 zero;
        vec2_set(&center, 0.5f, 0.5f);
        vec2_set(&one, 1.0f, 1.0f);
        vec2_set(&zero, 0.0f, 0.0f);

        gs_effect_set_texture(cursor->cursor_atlas_param,
                              filter->cursor_fallback_texture);
        gs_effect_set_vec2(cursor->cursor_content_param, &center);
        gs_effect_set_vec2(cursor->cursor_asset_size_param, &one);
        gs_effect_set_vec2(cursor->cursor_hotspot_param, &zero);
        gs_effect_set_vec2(cursor->cursor_atlas_grid_param, &one);
        gs_effect_set_float(cursor->cursor_frame_param, 0.0f);
        gs_effect_set_float(cursor->cursor_size_param, 8.0f);
        filter->cursor_fallback_bound = true;
    }

    /* Always force hidden while fallback is selected.  This also neutralizes
     * stale cursor_visible state when a real atlas was used in a prior frame. */
    gs_effect_set_float(cursor->cursor_visible_param, 0.0f);
}

bool is_managed_scene_camera(ArZoomFilter *phase1, obs_source_t **scene_target)
{
    if (scene_target)
        *scene_target = nullptr;
    if (!phase1 || !phase1->context)
        return false;

    const char *filter_name = obs_source_get_name(phase1->context);
    if (!filter_name ||
        std::strcmp(filter_name, arzoom::kSceneCameraFilterName.data()) != 0) {
        return false;
    }

    obs_source_t *target = obs_filter_get_target(phase1->context);
    if (!target || !obs_scene_from_source(target))
        return false;

    if (scene_target)
        *scene_target = target;
    return true;
}

bool is_display_capture(obs_source_t *source)
{
    if (!source)
        return false;
    const char *id = obs_source_get_id(source);
    if (!id)
        return false;
    return std::strcmp(id, "monitor_capture") == 0 ||
           std::strstr(id, "monitor_capture") != nullptr;
}

arzoom::Vec2 transformed_box_corner(const matrix4 &transform,
                                    float x, float y)
{
    vec3 input;
    vec3 output;
    vec3_set(&input, x, y, 0.0f);
    vec3_transform(&output, &input, &transform);
    return {output.x, output.y};
}

struct SceneDisplayCapture {
    obs_sceneitem_t *item = nullptr;
    obs_source_t *source = nullptr;
    size_t count = 0;
};

bool find_display_capture_cb(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
    auto *result = static_cast<SceneDisplayCapture *>(param);
    if (!result || !item || !obs_sceneitem_visible(item))
        return true;

    obs_source_t *source = obs_sceneitem_get_source(item);
    if (!is_display_capture(source))
        return true;

    ++result->count;
    if (result->count == 1) {
        result->item = item;
        result->source = source;
    }
    return true;
}

bool single_fullscreen_display_capture(obs_source_t *scene_source,
                                       SceneDisplayCapture &capture)
{
    obs_scene_t *scene = scene_source ? obs_scene_from_source(scene_source)
                                      : nullptr;
    if (!scene)
        return false;

    capture = {};
    obs_scene_enum_items(scene, find_display_capture_cb, &capture);
    if (capture.count != 1 || !capture.item || !capture.source)
        return false;

    const float width = static_cast<float>(obs_source_get_width(scene_source));
    const float height = static_cast<float>(obs_source_get_height(scene_source));
    if (width <= 0.0f || height <= 0.0f)
        return false;

    matrix4 transform;
    obs_sceneitem_get_box_transform(capture.item, &transform);
    const arzoom::SceneMappingQuad quad{
        transformed_box_corner(transform, 0.0f, 0.0f),
        transformed_box_corner(transform, 1.0f, 0.0f),
        transformed_box_corner(transform, 0.0f, 1.0f),
        transformed_box_corner(transform, 1.0f, 1.0f),
    };
    return arzoom::scene_mapping_is_full_canvas(quad, width, height, 1.75f);
}

bool monitor_from_capture_source(obs_source_t *capture_source,
                                 MonitorDescriptor &resolved)
{
    if (!capture_source)
        return false;

    const auto monitors = enumerate_monitors();
    if (monitors.empty())
        return false;

    obs_data_t *settings = obs_source_get_settings(capture_source);
    if (settings) {
        const char *monitor_id = obs_data_get_string(settings, "monitor_id");
        if (monitor_id && *monitor_id && std::strcmp(monitor_id, "DUMMY") != 0) {
            const auto found = std::find_if(
                monitors.begin(), monitors.end(),
                [&](const MonitorDescriptor &candidate) {
                    return monitor_matches_setting(candidate, monitor_id);
                });
            if (found != monitors.end()) {
                resolved = *found;
                obs_data_release(settings);
                return true;
            }
        }

        if (obs_data_has_user_value(settings, "monitor")) {
            const int index = static_cast<int>(obs_data_get_int(settings, "monitor"));
            if (index >= 0 && static_cast<size_t>(index) < monitors.size()) {
                resolved = monitors[static_cast<size_t>(index)];
                obs_data_release(settings);
                return true;
            }
        }
        obs_data_release(settings);
    }

    const uint32_t width = obs_source_get_width(capture_source);
    const uint32_t height = obs_source_get_height(capture_source);
    const MonitorDescriptor *match = nullptr;
    size_t matches = 0;
    for (const auto &candidate : monitors) {
        if ((static_cast<uint32_t>(candidate.width()) == width &&
             static_cast<uint32_t>(candidate.height()) == height) ||
            (static_cast<uint32_t>(candidate.width()) == height &&
             static_cast<uint32_t>(candidate.height()) == width)) {
            match = &candidate;
            ++matches;
        }
    }
    if (matches == 1 && match) {
        resolved = *match;
        return true;
    }
    return false;
}

bool capture_cursor_enabled(obs_source_t *capture_source)
{
    if (!capture_source)
        return false;
    obs_data_t *settings = obs_source_get_settings(capture_source);
    if (!settings)
        return false;
    const bool enabled = obs_data_has_user_value(settings, "capture_cursor")
                             ? obs_data_get_bool(settings, "capture_cursor")
                             : true;
    obs_data_release(settings);
    return enabled;
}

void apply_scene_mapping(Phase4SceneFilter *filter)
{
    ArZoomFilter *phase1 = phase1_from_phase4(filter);
    obs_source_t *scene_source = nullptr;
    if (!is_managed_scene_camera(phase1, &scene_source))
        return;

    const uint32_t width = std::max(obs_source_get_width(scene_source), 1u);
    const uint32_t height = std::max(obs_source_get_height(scene_source), 1u);

    /* Suppress P1's direct-target monitor heuristic for this tick. Scene Camera
     * owns mapping because the filter target is the scene, not Display Capture. */
    phase1->monitor_refresh_elapsed = 0.0f;
    phase1->last_source_width = width;
    phase1->last_source_height = height;
    phase1->monitor_dirty.store(false, std::memory_order_release);

    SceneDisplayCapture capture;
    MonitorDescriptor monitor;
    const bool mapped =
        single_fullscreen_display_capture(scene_source, capture) &&
        monitor_from_capture_source(capture.source, monitor);

    filter->mapping_valid = mapped;
    phase1->monitor_valid = mapped;
    if (mapped) {
        phase1->monitor = monitor;
        phase1->monitor_warning_logged = false;
        filter->mapping_warning_logged = false;
        if (phase1->last_logged_monitor != monitor.label) {
            phase1->last_logged_monitor = monitor.label;
            blog(LOG_INFO,
                 "[ArZoom] Scene Camera mapping proven: fullscreen Display Capture -> %s",
                 monitor.label.c_str());
        }

        if (capture_cursor_enabled(capture.source) &&
            !filter->nested_cursor_warning_logged) {
            blog(LOG_WARNING,
                 "[ArZoom] Scene Camera: Display Capture native cursor is enabled. "
                 "Turn it off when using an ArZoom Presentation Cursor to avoid double cursor.");
            filter->nested_cursor_warning_logged = true;
        } else if (!capture_cursor_enabled(capture.source)) {
            filter->nested_cursor_warning_logged = false;
        }
        return;
    }

    if (!filter->mapping_warning_logged) {
        blog(LOG_WARNING,
             "[ArZoom] Scene Camera Smart Follow mapping unavailable. "
             "Use exactly one visible fullscreen Display Capture for pointer-driven follow; "
             "presenter zoom/freeze/overview remain safe without guessed coordinates.");
        filter->mapping_warning_logged = true;
    }
}

void phase4_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    if (!filter || !filter->phase354)
        return;

    ArZoomFilter *phase1 = phase1_from_phase4(filter);
    obs_source_t *scene_source = nullptr;
    if (is_managed_scene_camera(phase1, &scene_source)) {
        filter->mapping_refresh_elapsed += std::clamp(seconds, 0.0f, 0.10f);
        if (filter->mapping_refresh_elapsed >= kSceneMappingRefreshSeconds) {
            filter->mapping_refresh_elapsed = 0.0f;
            apply_scene_mapping(filter);
        } else {
            /* Keep the scene-specific mapping owner authoritative between
             * refreshes so P1 never falls back to scene-size guessing. */
            phase1->monitor_refresh_elapsed = 0.0f;
            phase1->last_source_width =
                std::max(obs_source_get_width(scene_source), 1u);
            phase1->last_source_height =
                std::max(obs_source_get_height(scene_source), 1u);
            phase1->monitor_dirty.store(false, std::memory_order_release);
            phase1->monitor_valid = filter->mapping_valid;
        }
    }

    phase354_tick(filter->phase354, seconds);
}

void phase4_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    if (!filter || !filter->phase354)
        return;
    phase354_update(filter->phase354, settings);
    filter->mapping_refresh_elapsed = 1.0f;
}

obs_properties_t *phase4_properties(void *data)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    return phase354_properties(filter ? filter->phase354 : nullptr);
}

void phase4_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    if (!filter || !filter->phase354)
        return;

    /* Do this before the inherited P2/P3.5 renderer can activate the shared
     * effect for the first click.  The fallback is fully transparent, so there
     * is no visual or sampling cost beyond a single 1x1 texture binding when
     * the cursor state transitions to not-ready/inactive. */
    prime_cursor_sampler_safety(filter);
    phase354_render(filter->phase354, effect);
}

void phase4_deactivate(void *data)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    if (filter && filter->phase354)
        phase354_deactivate(filter->phase354);
}

void phase4_destroy(void *data)
{
    auto *filter = static_cast<Phase4SceneFilter *>(data);
    if (!filter)
        return;

    gs_texture_t *fallback = filter->cursor_fallback_texture;
    filter->cursor_fallback_texture = nullptr;
    phase354_destroy(filter->phase354);
    destroy_cursor_texture(fallback);
    delete filter;
}

void *phase4_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase354 = static_cast<Phase354Filter *>(
        phase354_create(settings, context));
    if (!phase354)
        return nullptr;

    auto *filter = new (std::nothrow) Phase4SceneFilter();
    if (!filter) {
        phase354_destroy(phase354);
        return nullptr;
    }
    filter->phase354 = phase354;
    filter->cursor_fallback_texture = create_transparent_cursor_fallback();
    if (!filter->cursor_fallback_texture) {
        blog(LOG_WARNING,
             "[ArZoom] Could not allocate transparent cursor safety texture; "
             "first-pass sampler protection is unavailable.");
    }

    ArZoomFilter *phase1 = phase1_from_phase4(filter);
    obs_source_t *scene_source = nullptr;
    if (is_managed_scene_camera(phase1, &scene_source)) {
        apply_scene_mapping(filter);
        blog(LOG_INFO,
             "[ArZoom] Phase 4 native Scene Camera runtime ready on scene '%s'",
             obs_source_get_name(scene_source));
    }
    return filter;
}

struct Phase4SourceInfoOverride {
    Phase4SourceInfoOverride()
    {
        arzoom_filter_info.create = phase4_create;
        arzoom_filter_info.destroy = phase4_destroy;
        arzoom_filter_info.video_tick = phase4_tick;
        arzoom_filter_info.video_render = phase4_render;
        arzoom_filter_info.update = phase4_update;
        arzoom_filter_info.get_properties = phase4_properties;
        arzoom_filter_info.get_defaults = phase352_defaults;
        arzoom_filter_info.deactivate = phase4_deactivate;
    }
};

Phase4SourceInfoOverride phase4_source_info_override;

} // namespace
