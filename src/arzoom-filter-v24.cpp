#include "arzoom-filter-v23.cpp"
#include "arzoom-spotlight-zoom-resize.hpp"
#include "arzoom-shortcut-observer.hpp"
#include "arzoom-shortcut-timeline.hpp"
#include "arzoom-keycap-rasterizer.hpp"

#define SETTING_KEYCAP_VISUAL "shortcut_keycaps_enabled"

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

    /* One optional global input owner, scoped per enabled OBS filter.
     * Video tick owns timeline; render owns graphics resources. */
    std::atomic<bool> keycaps_enabled{false};
    arzoom::shortcut::Timeline keycaps{};
    arzoom::presentation::KeycapRasterizer keycap_rasterizer{};
    uint32_t uploaded_generation = 0;
    gs_texture_t *keycap_texture = nullptr;
    gs_texture_t *keycap_fallback = nullptr;
    gs_eparam_t *keycap_atlas_param = nullptr;
    gs_eparam_t *keycap_size_param = nullptr;
    gs_eparam_t *keycap_opacity_param = nullptr;
    bool keycap_shader_ready = false;
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

    if (wrapper->keycaps_enabled.load(std::memory_order_acquire) &&
        phase1->enabled.load(std::memory_order_acquire)) {
        wrapper->keycaps.step(
            arzoom::shortcut::snapshot(), std::clamp(seconds, 0.0f, 0.10f));
    } else {
        wrapper->keycaps.reset();
    }

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

    const bool next_keycaps =
        obs_data_get_bool(settings, SETTING_KEYCAP_VISUAL);
    const bool prior_keycaps = wrapper->keycaps_enabled.exchange(
        next_keycaps, std::memory_order_acq_rel);
    if (next_keycaps != prior_keycaps) {
        if (next_keycaps)
            arzoom::shortcut::subscribe();
        else {
            arzoom::shortcut::unsubscribe();
            wrapper->keycaps.reset();
        }
    }

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
    if (!wrapper || !wrapper->phase23) return;
    Phase51Filter *phase51 = wrapper->phase23->phase51;
    ArZoomFilter *phase1 = phase24_phase1(wrapper);
    if (!phase1 || !phase1->context) return;

    /* P0 D3D11 invariant: every declared texture sampler must have a valid,
     * live fallback before ANY shared Draw, even if this effect is Off.
     * Missing optional resources fail safely to source pass-through. */
    if (!wrapper->keycap_shader_ready || !wrapper->keycap_fallback) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    gs_texture_t *texture = wrapper->keycap_fallback;
    float opacity = 0.0f;
    const bool active =
        wrapper->keycaps_enabled.load(std::memory_order_acquire) &&
        phase1->enabled.load(std::memory_order_acquire) &&
        wrapper->keycaps.active();

    if (active) {
        const auto &frame = wrapper->keycaps.frame();
        if (frame.generation != wrapper->uploaded_generation) {
            if (!wrapper->keycap_texture) {
                wrapper->keycap_texture = gs_texture_create(
                    arzoom::presentation::KeycapRasterizer::kWidth,
                    arzoom::presentation::KeycapRasterizer::kHeight,
                    GS_RGBA, 1, nullptr, GS_DYNAMIC);
            }
            if (wrapper->keycap_texture) {
                const QImage &raster = wrapper->keycap_rasterizer.image(frame);
                if (!raster.isNull()) {
                    gs_texture_set_image(
                        wrapper->keycap_texture, raster.constBits(),
                        static_cast<uint32_t>(raster.bytesPerLine()), false);
                    wrapper->uploaded_generation = frame.generation;
                }
            }
        }
        if (wrapper->keycap_texture &&
            wrapper->uploaded_generation == frame.generation) {
            texture = wrapper->keycap_texture;
            opacity = wrapper->keycaps.opacity();
        }
    }

    gs_effect_set_texture(wrapper->keycap_atlas_param, texture);
    vec2 dimensions;
    vec2_set(&dimensions,
             arzoom::presentation::KeycapRasterizer::kWidth,
             arzoom::presentation::KeycapRasterizer::kHeight);
    gs_effect_set_vec2(wrapper->keycap_size_param, &dimensions);
    gs_effect_set_float(wrapper->keycap_opacity_param, opacity);

    /* Keycap-only is still one shared PSZoom pass. The accepted P5 solo
     * render helper already primes zoom, click-neutral, cursor fallback,
     * Spotlight and the OBS pass. No additional fullscreen draw is needed. */
    if (opacity > 0.005f &&
        !phase21_presentation_required(phase51)) {
        if (!phase21_prime_neutral_spotlight_abi(phase51) ||
            !wrapper->phase23->cinematic_shader_ready) {
            obs_source_skip_video_filter(phase1->context);
            return;
        }
        gs_effect_set_float(
            wrapper->phase23->cinematic_scale_param, 1.0f);
        gs_effect_set_float(
            wrapper->phase23->cinematic_dim_mix_param, 1.0f);
        phase18_spotlight_only_render(
            phase51, phase1, phase51_phase2(phase51),
            phase51_phase35(phase51));
        return;
    }

    phase23_render(wrapper->phase23, effect);
}

obs_properties_t *phase24_properties(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    obs_properties_t *props =
        phase23_properties(wrapper ? wrapper->phase23 : nullptr);
    if (!props) return nullptr;
    obs_properties_t *group = obs_properties_create();
    obs_properties_add_bool(
        group, SETTING_KEYCAP_VISUAL,
        obs_module_text("ArZoom.Keycaps.Enabled"));
    obs_property_t *info = obs_properties_add_text(
        group, "shortcut_keycap_info",
        obs_module_text("ArZoom.Keycaps.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);
    obs_properties_add_group(
        props, "shortcut_keycap_group",
        obs_module_text("ArZoom.Keycaps.Group"),
        OBS_GROUP_NORMAL, group);
    return props;
}

void phase24_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper || !wrapper->phase23)
        return;
    wrapper->spotlight_resize.reset();
    wrapper->keycaps.reset();
    phase23_deactivate(wrapper->phase23);
}

void phase24_destroy(void *data)
{
    auto *wrapper = static_cast<Phase24Filter *>(data);
    if (!wrapper)
        return;
    if (wrapper->keycaps_enabled.exchange(
            false, std::memory_order_acq_rel)) {
        arzoom::shortcut::unsubscribe();
    }
    /* The OBS callback is not guaranteed to hold a graphics context. */
    obs_enter_graphics();
    if (wrapper->keycap_texture)
        gs_texture_destroy(wrapper->keycap_texture);
    if (wrapper->keycap_fallback)
        gs_texture_destroy(wrapper->keycap_fallback);
    obs_leave_graphics();
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

    ArZoomFilter *phase1 = phase24_phase1(wrapper);
    if (phase1 && phase1->effect) {
        wrapper->keycap_atlas_param = gs_effect_get_param_by_name(
            phase1->effect, "keycap_atlas");
        wrapper->keycap_size_param = gs_effect_get_param_by_name(
            phase1->effect, "keycap_size_px");
        wrapper->keycap_opacity_param = gs_effect_get_param_by_name(
            phase1->effect, "keycap_opacity");
        wrapper->keycap_shader_ready =
            wrapper->keycap_atlas_param && wrapper->keycap_size_param &&
            wrapper->keycap_opacity_param;
    }

    /* Permanent transparent atlas prevents D3D11 undeclared-sampler
     * black frames on camera/cursor/Spotlight first use and all idle passes. */
    const uint8_t transparent[4] = {0, 0, 0, 0};
    const uint8_t *pixels = transparent;
    obs_enter_graphics();
    wrapper->keycap_fallback =
        gs_texture_create(1, 1, GS_RGBA, 1, &pixels, 0);
    obs_leave_graphics();

    if (obs_data_get_bool(settings, SETTING_KEYCAP_VISUAL)) {
        wrapper->keycaps_enabled.store(true, std::memory_order_release);
        arzoom::shortcut::subscribe();
    }
    if (!wrapper->keycap_shader_ready || !wrapper->keycap_fallback) {
        blog(LOG_WARNING,
             "[ArZoom] Keycap shader/safety atlas not ready; "
             "shared video pass safely disabled until repaired");
    }

    blog(LOG_INFO,
         "[ArZoom] P5.6 Zoom +/- resize-only Spotlight coupling ready");
    return wrapper;
}

void phase24_defaults(obs_data_t *settings)
{
    phase23_defaults(settings);
    obs_data_set_default_bool(settings, SETTING_KEYCAP_VISUAL, false);
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
        arzoom_filter_info.get_defaults = phase24_defaults;
        arzoom_filter_info.deactivate = phase24_deactivate;
    }
};

Phase24SourceInfoOverride phase24_source_info_override;

} // namespace
