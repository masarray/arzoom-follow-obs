#include "arzoom-filter-v23.cpp"
#include "arzoom-spotlight-zoom-resize.hpp"

/*
 * P5.6 Zoom +/- intent separation
 * =================================
 *
 * Direct OBS v23 acceptance proved Toggle Zoom ON/OFF cinematic choreography
 * is correct, but exposed a distinct presenter-intent bug: Zoom +/- persists
 * `zoom_amount` through obs_source_update(), and v23 treated every settings
 * update as a reason to reset the cinematic state. That replayed the iris
 * close animation after every Increase/Decrease Zoom command.
 *
 * v24 makes the contract explicit:
 *   Toggle Zoom ON/OFF -> cinematic full-frame close/open choreography.
 *   Zoom +/- while ON   -> resize-only camera/cursor/Spotlight motion.
 *
 * Presentation Cursor already scales from live current_zoom in accepted P4.1.
 * This wrapper adds the corresponding bounded Spotlight resize multiplier and
 * follows the actual live camera progress. It never writes camera intent.
 */
namespace {

struct Phase24Filter {
    Phase23Filter *phase23 = nullptr;
    arzoom::SpotlightZoomResizeState spotlight_resize{};
};

ArZoomFilter *phase24_phase1(Phase24Filter *filter)
{
    return filter && filter->phase23 && filter->phase23->phase51
               ? phase51_phase1(filter->phase23->phase51)
               : nullptr;
}

void phase24_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper || !wrapper->phase23)
        return;

    Phase23Filter *phase23 = wrapper->phase23;
    phase23_tick(phase23, seconds);

    ArZoomFilter *phase1 = phase24_phase1(wrapper);
    if (!phase1)
        return;

    const bool zoom_requested =
        phase1->requested_zoom.load(std::memory_order_acquire);
    const float configured_zoom =
        phase1->configured_zoom.load(std::memory_order_acquire);
    const float live_zoom = std::max(phase1->current_zoom, 1.0f);

    wrapper->spotlight_resize.observe(
        zoom_requested, configured_zoom, live_zoom);
    wrapper->spotlight_resize.step(live_zoom);

    /* The initial Toggle-Zoom cinematic path remains byte-for-byte v23 because
     * resize scale is neutral until configured_zoom changes inside the active
     * session. Once Zoom +/- is used, multiply only the working aperture scale
     * by the resize factor. The camera's own live trajectory supplies timing. */
    if (zoom_requested && phase23->cinematic_render_active) {
        phase23->cinematic_render_scale = std::clamp(
            phase23->cinematic_render_scale *
                wrapper->spotlight_resize.scale,
            0.35f, 64.0f);
    }
}

void phase24_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper || !wrapper->phase23 || !wrapper->phase23->phase51)
        return;

    Phase23Filter *phase23 = wrapper->phase23;
    phase23_migrate_defaults(settings);

    /* Preserve the complete inherited settings path, but deliberately bypass
     * phase23_update(): that function resets cinematic choreography for every
     * obs_source_update(), including the persistence write used by Zoom +/-.
     * Mode/master lifecycle is still owned by phase52/phase51. */
    phase52_update(phase23->phase51, settings);

    const bool previous_link =
        phase23->link_to_zoom.load(std::memory_order_acquire);
    const bool next_link =
        obs_data_get_bool(settings, SETTING_SPOTLIGHT_LINK_ZOOM);
    phase23->link_to_zoom.store(next_link, std::memory_order_release);

    phase23->cinematic_speed.store(
        static_cast<int>(parse_cinematic_speed(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_CINEMATIC_SPEED))),
        std::memory_order_release);

    /* Only changing the cinematic-link topology requires a clean choreography
     * reset. Zoom amount, dim, feather, mode, cursor style and unrelated OBS
     * settings must never replay the full-frame iris transition. */
    if (previous_link != next_link) {
        phase23->reset_requested.store(true, std::memory_order_release);
        wrapper->spotlight_resize.reset();
    }
}

void phase24_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (wrapper && wrapper->phase23)
        phase23_render(wrapper->phase23, effect);
}

obs_properties_t *phase24_properties(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    return phase23_properties(wrapper ? wrapper->phase23 : nullptr);
}

void phase24_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper || !wrapper->phase23)
        return;
    wrapper->spotlight_resize.reset();
    phase23_deactivate(wrapper->phase23);
}

void phase24_destroy(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper)
        return;
    phase23_destroy(wrapper->phase23);
    delete wrapper;
}

void *phase24_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase23 = static_cast<Phase23Filter *>(
        phase23_create(settings, context));
    if (!phase23)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase24Filter();
    if (!wrapper) {
        phase23_destroy(phase23);
        return nullptr;
    }
    wrapper->phase23 = phase23;
    wrapper->spotlight_resize.reset();

    blog(LOG_INFO,
         "[ArZoom] P5.6 Zoom +/- resize-only Spotlight coupling ready");
    return wrapper;
}

struct Phase24SourceInfoOverride {
    Phase24SourceInfoOverride()
    {
        arzoom_filter_info.create = phase24_create;
        arzoom_filter_info.destroy = phase24_destroy;
        arzoom_filter_info.video_tick = phase24_tick;
        arzoom_filter_info.video_render = phase24_render;
        arzoom_filter_info.update = phase24_update;
        arzoom_filter_info.get_properties = phase24_properties;
        arzoom_filter_info.get_defaults = phase23_defaults;
        arzoom_filter_info.deactivate = phase24_deactivate;
    }
};

Phase24SourceInfoOverride phase24_source_info_override;

} // namespace
