#include "arzoom-filter-v17.cpp"

/*
 * P5.2 render-ownership recovery
 * ==============================
 *
 * Direct OBS trial #2 proved that allowing P5 to duplicate the complete
 * camera/click/Presentation-Cursor renderer was architecturally unsafe.  A
 * Display Capture click could flicker black and changing the cursor asset could
 * leave the filter output fully black until the filter was disabled/deleted.
 *
 * The accepted P4.1 invariant is restored here:
 *   - phase41_cursor_scaled_render()/phase35_render() remains the sole owner of
 *     frames that already need the presentation pass for camera/click/cursor;
 *   - Spotlight only publishes analytic-mask uniforms before delegating;
 *   - only a true Spotlight-only 1x frame gets a tiny dedicated pass, where no
 *     real cursor atlas or click event is owned by P5 at all.
 *
 * This deliberately bypasses phase5_render()/phase51_render() in production.
 * They remain in the include chain only as trial-history/state helpers until
 * the P5 branch is squashed after direct OBS acceptance.
 */
namespace {

bool phase18_cursor_renderable(Phase51Filter *filter)
{
    Phase35Filter *cursor = phase51_phase35(filter);
    if (!cursor ||
        !cursor->cursor_enabled.load(std::memory_order_acquire) ||
        !cursor->cursor_position_valid.load(std::memory_order_acquire) ||
        !cursor->cursor_shader_ready) {
        return false;
    }

    std::lock_guard<std::mutex> lock(cursor->asset_mutex);
    return cursor->atlas_texture != nullptr &&
           cursor->frame_width > 0 && cursor->frame_height > 0 &&
           cursor->frame_count > 0;
}

bool phase18_camera_active(ArZoomFilter *phase1)
{
    return phase1 &&
           (phase1->current_zoom > 1.0005f ||
            !arzoom::nearly_equal(phase1->current_center,
                                  {0.5f, 0.5f}, 0.0005f));
}

bool phase18_click_active(Phase2Filter *phase2)
{
    return phase2 &&
           phase2->click_visual_enabled.load(std::memory_order_acquire) &&
           phase2->click_shader_ready && phase2->clicks.has_active();
}

void phase18_prepare_spotlight_uniforms(Phase51Filter *filter,
                                        float width, float height)
{
    if (!filter || !filter->phase5)
        return;
    phase51_set_extension_uniforms(filter);
    phase5_set_spotlight_uniforms(filter->phase5, width, height);
}

void phase18_clear_click_uniforms(Phase2Filter *phase2)
{
    if (!phase2)
        return;
    const arzoom::ClickEvent no_click{};
    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i)
        set_click_uniform(phase2->click_params[i], no_click);
}

void phase18_spotlight_only_render(Phase51Filter *filter,
                                   ArZoomFilter *phase1,
                                   Phase2Filter *phase2,
                                   Phase35Filter *cursor)
{
    if (!filter || !filter->phase5 || !phase1 || !phase2 || !cursor ||
        !phase1->effect_ready || !phase1->effect ||
        !phase1->enabled.load(std::memory_order_acquire)) {
        if (phase1 && phase1->context)
            obs_source_skip_video_filter(phase1->context);
        return;
    }

    /* The Spotlight-only route owns no real cursor resource.  Prime the exact
     * permanent transparent sampler accepted in v0.5.0 before activating the
     * effect, then explicitly keep the cursor hidden. */
    if (filter->phase5->phase41 && filter->phase5->phase41->phase4)
        prime_cursor_sampler_safety(filter->phase5->phase41->phase4);
    set_cursor_hidden(cursor);

    if (!obs_source_process_filter_begin(
            phase1->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    gs_effect_set_float(phase1->zoom_param,
                        std::max(phase1->current_zoom, 1.0f));
    vec2 camera_center;
    vec2_set(&camera_center, phase1->current_center.x,
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

    /* Effect uniforms persist across frames.  A solo Spotlight frame must
     * explicitly neutralize old click data and cursor visibility so a prior
     * presentation frame cannot leak stale feedback into this pass. */
    phase18_clear_click_uniforms(phase2);
    set_cursor_hidden(cursor);
    phase18_prepare_spotlight_uniforms(filter, width, height);

    obs_source_process_filter_end(
        phase1->context, phase1->effect, 0, 0);
}

void phase18_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter || !filter->phase5 || !filter->phase5->phase41)
        return;

    ArZoomFilter *phase1 = phase51_phase1(filter);
    Phase2Filter *phase2 = phase51_phase2(filter);
    Phase35Filter *cursor = phase51_phase35(filter);
    if (!phase1 || !phase2 || !cursor) {
        if (phase1 && phase1->context)
            obs_source_skip_video_filter(phase1->context);
        return;
    }

    const bool spotlight_active =
        filter->phase5->runtime_active.load(std::memory_order_acquire) &&
        filter->phase5->shader_ready && filter->extension_shader_ready;

    if (!spotlight_active) {
        /* Critical recovery invariant: Spotlight OFF means the exact accepted
         * P4.1 render owner, with no P5 render code between OBS and that path. */
        phase5_set_disabled_uniform(filter->phase5);
        phase41_cursor_scaled_render(filter->phase5->phase41, effect);
        return;
    }

    const bool camera_active = phase18_camera_active(phase1);
    const bool click_active = phase18_click_active(phase2);
    const bool cursor_renderable = phase18_cursor_renderable(filter);

    if (!arzoom::spotlight_needs_solo_pass(
            true, camera_active, click_active, cursor_renderable)) {
        /* P4.1 continues to own camera/click/cursor resources.  Spotlight only
         * publishes compact analytic-mask uniforms; no texture atlas is read,
         * swapped or locked by P5. */
        obs_source_t *target = obs_filter_get_target(phase1->context);
        const float width = static_cast<float>(
            target ? std::max(obs_source_get_width(target), 1u) : 1u);
        const float height = static_cast<float>(
            target ? std::max(obs_source_get_height(target), 1u) : 1u);
        phase18_prepare_spotlight_uniforms(filter, width, height);
        phase41_cursor_scaled_render(filter->phase5->phase41, effect);
        return;
    }

    phase18_spotlight_only_render(filter, phase1, phase2, cursor);
}

struct Phase18SourceInfoOverride {
    Phase18SourceInfoOverride()
    {
        /* Creation/update/tick/properties/hotkeys remain P5.1.  Only render
         * ownership changes in P5.2. */
        arzoom_filter_info.video_render = phase18_render;
    }
};

Phase18SourceInfoOverride phase18_source_info_override;

} // namespace
