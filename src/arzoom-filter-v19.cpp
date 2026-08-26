#include "arzoom-filter-v18.cpp"
#include "arzoom-presentation-pass-handshake.hpp"

/*
 * P5.3 / P0 render-lifecycle recovery
 * ===================================
 *
 * Direct OBS 32.2.2 trial #3 narrowed the remaining black-output regression:
 * the filter can recover when Toggle Spotlight forces a fresh processed frame.
 * That points at the transition between true pass-through/skip and the first
 * resource-bearing presentation frame, not at Spotlight geometry itself.
 *
 * This wrapper preserves P5.2 render ownership and adds only a bounded neutral
 * activation handshake.  A new/updated/reactivated filter, or the first frame
 * that needs camera/click/cursor/Spotlight after idle, gets at most three warm
 * processed frames before normal P5.2 routing resumes.
 *
 * Warm frames:
 *   - use the existing ArZoom effect and current camera transform;
 *   - explicitly disable Spotlight;
 *   - explicitly clear all click uniforms;
 *   - bind the permanent transparent cursor fallback directly;
 *   - force Presentation Cursor hidden;
 *   - never touch a real cursor atlas;
 *   - never mutate OBS scene items;
 *   - never accumulate unbounded work.
 */
namespace {

struct Phase19Filter {
    Phase51Filter *phase51 = nullptr;
    std::atomic<int> warm_frames{arzoom::kPresentationWarmFrameCount};

    /* video_tick-owned edge detector */
    bool last_presentation_required = false;
};

void phase19_request_warm_frames(
    Phase19Filter *filter,
    int requested = arzoom::kPresentationWarmFrameCount)
{
    if (!filter)
        return;

    int current = filter->warm_frames.load(std::memory_order_acquire);
    for (;;) {
        const int desired =
            arzoom::merge_presentation_warm_frames(current, requested);
        if (desired == current)
            return;
        if (filter->warm_frames.compare_exchange_weak(
                current, desired,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;
        }
    }
}

bool phase19_spotlight_active(Phase51Filter *filter)
{
    return filter && filter->phase5 &&
           filter->phase5->runtime_active.load(std::memory_order_acquire) &&
           filter->phase5->shader_ready && filter->extension_shader_ready;
}

bool phase19_presentation_required(Phase51Filter *filter)
{
    if (!filter)
        return false;
    return phase18_camera_active(phase51_phase1(filter)) ||
           phase18_click_active(phase51_phase2(filter)) ||
           phase18_cursor_renderable(filter) ||
           phase19_spotlight_active(filter);
}

bool phase19_bind_neutral_cursor_fallback(Phase51Filter *filter,
                                          Phase35Filter *cursor)
{
    if (!filter || !filter->phase5 || !filter->phase5->phase41 ||
        !filter->phase5->phase41->phase4 || !cursor ||
        !cursor->cursor_shader_ready) {
        return false;
    }

    Phase4SceneFilter *phase4 = filter->phase5->phase41->phase4;
    if (!phase4->cursor_fallback_texture)
        return false;

    vec2 center;
    vec2 one;
    vec2 zero;
    vec2_set(&center, 0.5f, 0.5f);
    vec2_set(&one, 1.0f, 1.0f);
    vec2_set(&zero, 0.0f, 0.0f);

    /* Do not ask cursor readiness whether fallback is required here.  The
     * whole point of the handshake is to make this frame independent from a
     * newly swapped/destroyed real atlas. */
    gs_effect_set_texture(cursor->cursor_atlas_param,
                          phase4->cursor_fallback_texture);
    gs_effect_set_vec2(cursor->cursor_content_param, &center);
    gs_effect_set_vec2(cursor->cursor_asset_size_param, &one);
    gs_effect_set_vec2(cursor->cursor_hotspot_param, &zero);
    gs_effect_set_vec2(cursor->cursor_atlas_grid_param, &one);
    gs_effect_set_float(cursor->cursor_frame_param, 0.0f);
    gs_effect_set_float(cursor->cursor_size_param, 8.0f);
    gs_effect_set_float(cursor->cursor_visible_param, 0.0f);
    phase4->cursor_fallback_bound = true;
    return true;
}

void phase19_neutral_warm_render(Phase19Filter *wrapper)
{
    Phase51Filter *filter = wrapper ? wrapper->phase51 : nullptr;
    ArZoomFilter *phase1 = phase51_phase1(filter);
    Phase2Filter *phase2 = phase51_phase2(filter);
    Phase35Filter *cursor = phase51_phase35(filter);

    if (!filter || !filter->phase5 || !phase1 || !phase2 || !cursor ||
        !phase1->context || !phase1->effect_ready || !phase1->effect ||
        !phase1->enabled.load(std::memory_order_acquire)) {
        if (phase1 && phase1->context)
            obs_source_skip_video_filter(phase1->context);
        return;
    }

    phase5_set_disabled_uniform(filter->phase5);
    phase18_clear_click_uniforms(phase2);
    set_cursor_hidden(cursor);

    if (!phase19_bind_neutral_cursor_fallback(filter, cursor)) {
        /* Optional presentation effects must never risk a black source when
         * the permanent safety resource itself is unavailable. */
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    if (!obs_source_process_filter_begin(
            phase1->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    /* Preserve accepted camera continuity if a zoom transition itself caused
     * activation. At idle these values are exactly the identity transform. */
    gs_effect_set_float(phase1->zoom_param,
                        std::max(phase1->current_zoom, 1.0f));
    vec2 camera_center;
    vec2_set(&camera_center,
             phase1->current_center.x,
             phase1->current_center.y);
    gs_effect_set_vec2(phase1->center_param, &camera_center);

    obs_source_t *target = obs_filter_get_target(phase1->context);
    const float width = static_cast<float>(
        target ? std::max(obs_source_get_width(target), 1u) : 1u);
    const float height = static_cast<float>(
        target ? std::max(obs_source_get_height(target), 1u) : 1u);
    if (phase2->viewport_size_param) {
        vec2 viewport;
        vec2_set(&viewport, width, height);
        gs_effect_set_vec2(phase2->viewport_size_param, &viewport);
    }

    /* process_filter_begin may change graphics state; set neutral presentation
     * uniforms again immediately before the draw so stale prior-frame values
     * cannot leak into the handshake frame. */
    phase5_set_disabled_uniform(filter->phase5);
    phase18_clear_click_uniforms(phase2);
    phase19_bind_neutral_cursor_fallback(filter, cursor);
    set_cursor_hidden(cursor);

    obs_source_process_filter_end(
        phase1->context, phase1->effect, 0, 0);
}

void phase19_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    phase51_tick(wrapper->phase51, seconds);

    const bool required = phase19_presentation_required(wrapper->phase51);
    if (arzoom::presentation_pass_activation_edge(
            wrapper->last_presentation_required, required)) {
        phase19_request_warm_frames(wrapper);
    }
    wrapper->last_presentation_required = required;
}

void phase19_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    int pending = wrapper->warm_frames.load(std::memory_order_acquire);
    if (pending > 0) {
        phase19_neutral_warm_render(wrapper);
        wrapper->warm_frames.store(
            arzoom::consume_presentation_warm_frame(pending),
            std::memory_order_release);
        return;
    }

    phase18_render(wrapper->phase51, effect);
}

void phase19_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    /* Resource replacement happens inside the inherited update.  The next
     * rendered frame is therefore forced through the neutral handshake rather
     * than immediately consuming the new real atlas/click state. */
    phase52_update(wrapper->phase51, settings);
    phase19_request_warm_frames(wrapper);
}

obs_properties_t *phase19_properties(void *data)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    return phase51_properties(wrapper ? wrapper->phase51 : nullptr);
}

void phase19_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    if (!wrapper || !wrapper->phase51)
        return;

    phase51_deactivate(wrapper->phase51);
    wrapper->last_presentation_required = false;
    wrapper->warm_frames.store(
        arzoom::kPresentationWarmFrameCount,
        std::memory_order_release);
}

void phase19_destroy(void *data)
{
    auto *wrapper = static_cast<Phase19Filter *>(data);
    if (!wrapper)
        return;
    phase51_destroy(wrapper->phase51);
    delete wrapper;
}

void *phase19_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase51 = static_cast<Phase51Filter *>(
        phase52_create(settings, context));
    if (!phase51)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase19Filter();
    if (!wrapper) {
        phase51_destroy(phase51);
        return nullptr;
    }

    wrapper->phase51 = phase51;
    wrapper->warm_frames.store(
        arzoom::kPresentationWarmFrameCount,
        std::memory_order_release);
    wrapper->last_presentation_required = false;

    blog(LOG_INFO,
         "[ArZoom] P0 bounded presentation-pass activation handshake ready");
    return wrapper;
}

struct Phase19SourceInfoOverride {
    Phase19SourceInfoOverride()
    {
        arzoom_filter_info.create = phase19_create;
        arzoom_filter_info.destroy = phase19_destroy;
        arzoom_filter_info.video_tick = phase19_tick;
        arzoom_filter_info.video_render = phase19_render;
        arzoom_filter_info.update = phase19_update;
        arzoom_filter_info.get_properties = phase19_properties;
        arzoom_filter_info.get_defaults = phase51_defaults;
        arzoom_filter_info.deactivate = phase19_deactivate;
    }
};

Phase19SourceInfoOverride phase19_source_info_override;

} // namespace
