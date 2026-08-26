#include "arzoom-filter-v19.cpp"
#include "arzoom-cursor-sampler-guard.hpp"

/*
 * P5.4 / P0 cursor-sampler ordering recovery
 * ==========================================
 *
 * Direct OBS 32.2.2 trial #4 disproved the warm-frame hypothesis by itself:
 * the symptoms were unchanged.  Audit then found a lower-level ordering bug in
 * the accepted shared presentation pass.  When a real Presentation Cursor was
 * considered ready, phase35_render() entered obs_source_process_filter_begin()
 * before binding cursor_atlas to the real atlas.  The P4 safety primer also
 * deliberately skipped the transparent fallback in that ready state.
 *
 * Therefore the graphics backend could enter a processed filter frame while
 * cursor_atlas still referenced an old/destroyed texture or had not yet been
 * rebound.  A later gs_effect_set_texture() in the same render callback was too
 * late for backends that validate declared samplers at process-filter begin.
 * Toggle Spotlight appeared to "repair" the source because its neutral path
 * explicitly bound the permanent fallback before process_filter_begin().
 *
 * v20 establishes one hard invariant around the existing renderer:
 *
 *   EVERY processed ArZoom frame has a live cursor_atlas texture bound before
 *   any inherited path can call obs_source_process_filter_begin().
 *
 * It also serializes rare settings/resource updates against video_render so a
 * cursor style replacement cannot destroy the texture between prebind and the
 * draw.  No extra GPU pass, scene render, frame readback or always-render idle
 * path is introduced.
 */
namespace {

struct Phase20Filter {
    Phase19Filter *phase19 = nullptr;

    /* Settings changes are rare; video rendering is frequent.  This mutex is
     * uncontended in steady state and only closes the texture-swap race during
     * cursor/presentation settings updates. */
    std::mutex render_update_mutex;
};

Phase51Filter *phase20_phase51(Phase20Filter *wrapper)
{
    return wrapper && wrapper->phase19 ? wrapper->phase19->phase51 : nullptr;
}

bool phase20_prebind_cursor_sampler(Phase20Filter *wrapper)
{
    Phase51Filter *filter = phase20_phase51(wrapper);
    if (!filter || !filter->phase5 || !filter->phase5->phase41 ||
        !filter->phase5->phase41->phase4) {
        return true;
    }

    Phase35Filter *cursor = phase51_phase35(filter);
    Phase4SceneFilter *phase4 = filter->phase5->phase41->phase4;
    if (!cursor || !cursor->cursor_shader_ready ||
        !cursor->cursor_atlas_param) {
        /* Lower layers already fail safe when the cursor shader ABI itself is
         * unavailable.  There is no usable sampler parameter to guard here. */
        return true;
    }

    bool atlas_ready = false;
    bool cursor_enabled =
        cursor->cursor_enabled.load(std::memory_order_acquire);
    bool position_valid =
        cursor->cursor_position_valid.load(std::memory_order_acquire);

    std::lock_guard<std::mutex> asset_lock(cursor->asset_mutex);
    atlas_ready = cursor->atlas_texture != nullptr &&
                  cursor->frame_width > 0 && cursor->frame_height > 0 &&
                  cursor->frame_count > 0;

    const auto route = arzoom::cursor_sampler_route(
        cursor_enabled, position_valid, cursor->cursor_shader_ready,
        atlas_ready);

    if (route == arzoom::CursorSamplerRoute::RealAtlas) {
        /* Critical ordering: real atlas is live and bound BEFORE the inherited
         * renderer is allowed to enter process_filter_begin().  The outer
         * render/update mutex prevents settings code from swapping/destroying
         * this texture until the inherited draw completes. */
        gs_effect_set_texture(cursor->cursor_atlas_param,
                              cursor->atlas_texture);
        phase4->cursor_fallback_bound = false;
        return true;
    }

    if (!phase4->cursor_fallback_texture)
        return false;

    vec2 center;
    vec2 one;
    vec2 zero;
    vec2_set(&center, 0.5f, 0.5f);
    vec2_set(&one, 1.0f, 1.0f);
    vec2_set(&zero, 0.0f, 0.0f);

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

void phase20_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    if (!wrapper || !wrapper->phase19)
        return;
    phase19_tick(wrapper->phase19, seconds);
}

void phase20_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    if (!wrapper || !wrapper->phase19)
        return;

    std::lock_guard<std::mutex> transition_lock(wrapper->render_update_mutex);

    if (!phase20_prebind_cursor_sampler(wrapper)) {
        /* A declared cursor sampler with no live fallback is not allowed to
         * enter the effect.  Preserve the source instead of risking black. */
        Phase51Filter *filter = phase20_phase51(wrapper);
        ArZoomFilter *phase1 = phase51_phase1(filter);
        if (filter && filter->phase5)
            phase5_set_disabled_uniform(filter->phase5);
        if (phase1 && phase1->context)
            obs_source_skip_video_filter(phase1->context);
        return;
    }

    phase19_render(wrapper->phase19, effect);
}

void phase20_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    if (!wrapper || !wrapper->phase19)
        return;

    /* Serialize the entire inherited update because cursor style/custom asset
     * replacement creates/swaps/destroys gs_texture_t resources inside it.
     * Request the existing bounded neutral frames before and after mutation;
     * the first post-update render will then prebind a guaranteed live sampler
     * before any process_filter_begin(). */
    std::lock_guard<std::mutex> transition_lock(wrapper->render_update_mutex);
    phase19_request_warm_frames(wrapper->phase19);
    phase19_update(wrapper->phase19, settings);
    phase19_request_warm_frames(wrapper->phase19);
}

obs_properties_t *phase20_properties(void *data)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    return phase19_properties(wrapper ? wrapper->phase19 : nullptr);
}

void phase20_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    if (!wrapper || !wrapper->phase19)
        return;
    std::lock_guard<std::mutex> transition_lock(wrapper->render_update_mutex);
    phase19_deactivate(wrapper->phase19);
}

void phase20_destroy(void *data)
{
    auto *wrapper = static_cast<Phase20Filter *>(data);
    if (!wrapper)
        return;
    {
        std::lock_guard<std::mutex> transition_lock(
            wrapper->render_update_mutex);
        phase19_destroy(wrapper->phase19);
        wrapper->phase19 = nullptr;
    }
    delete wrapper;
}

void *phase20_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase19 = static_cast<Phase19Filter *>(
        phase19_create(settings, context));
    if (!phase19)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase20Filter();
    if (!wrapper) {
        phase19_destroy(phase19);
        return nullptr;
    }
    wrapper->phase19 = phase19;

    blog(LOG_INFO,
         "[ArZoom] P0 cursor sampler prebind + render/update guard ready");
    return wrapper;
}

struct Phase20SourceInfoOverride {
    Phase20SourceInfoOverride()
    {
        arzoom_filter_info.create = phase20_create;
        arzoom_filter_info.destroy = phase20_destroy;
        arzoom_filter_info.video_tick = phase20_tick;
        arzoom_filter_info.video_render = phase20_render;
        arzoom_filter_info.update = phase20_update;
        arzoom_filter_info.get_properties = phase20_properties;
        arzoom_filter_info.get_defaults = phase51_defaults;
        arzoom_filter_info.deactivate = phase20_deactivate;
    }
};

Phase20SourceInfoOverride phase20_source_info_override;

} // namespace
