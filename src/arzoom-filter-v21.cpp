#include "arzoom-filter-v18.cpp"
#include "arzoom-spotlight-abi.hpp"

/*
 * P5.4 / v21 — D3D11 Spotlight ABI initialization recovery
 * ========================================================
 *
 * Direct OBS log evidence finally identified the black-frame root cause:
 *
 *   device_draw (D3D11): Not all shader parameters were set
 *
 * The P5 shader added nine Spotlight-specific uniforms to the same Draw
 * technique used by the accepted camera/click/Presentation Cursor path.  When
 * Spotlight was OFF, the inherited P5 path only wrote spotlight_enabled = 0.
 * The remaining Spotlight uniforms were never initialized before a processed
 * Draw triggered by click, zoom, or Presentation Cursor.  D3D11 therefore
 * rejected the draw.  Turning Spotlight ON happened to repair the frame because
 * the Spotlight-active route populated the complete ABI before Draw.
 *
 * v21 deliberately drops the speculative v19 warm-frame and v20 sampler-order
 * wrappers from the production include chain.  It returns to the single-owner
 * P5.2/v18 renderer and adds one evidence-backed invariant:
 *
 *   Every processed Draw starts with all P5-only shader uniforms initialized.
 *
 * Spotlight-active rendering then overwrites the neutral packet with the real
 * configured values inside phase18_render().  Steady pass-through frames do no
 * extra uniform work.
 */
namespace {

bool phase21_presentation_required(Phase51Filter *filter)
{
    if (!filter)
        return false;

    const bool spotlight_active =
        filter->phase5 &&
        filter->phase5->runtime_active.load(std::memory_order_acquire) &&
        filter->phase5->shader_ready && filter->extension_shader_ready;

    return phase18_camera_active(phase51_phase1(filter)) ||
           phase18_click_active(phase51_phase2(filter)) ||
           phase18_cursor_renderable(filter) || spotlight_active;
}

bool phase21_prime_neutral_spotlight_abi(Phase51Filter *filter)
{
    if (!filter || !filter->phase5)
        return false;

    Phase5Filter *phase5 = filter->phase5;
    if (!phase5->shader_ready || !filter->extension_shader_ready)
        return false;

    const arzoom::SpotlightNeutralAbi neutral = arzoom::spotlight_neutral_abi();

    vec2 center;
    vec2 half_size;
    vec2_set(&center, neutral.center_x, neutral.center_y);
    vec2_set(&half_size, neutral.half_size_x_px, neutral.half_size_y_px);

    gs_effect_set_float(phase5->enabled_param, neutral.enabled);
    gs_effect_set_vec2(phase5->center_param, &center);
    gs_effect_set_vec2(phase5->half_size_param, &half_size);
    gs_effect_set_float(phase5->feather_param, neutral.feather_px);
    gs_effect_set_float(phase5->dim_param, neutral.dim_strength);
    gs_effect_set_float(phase5->shape_param, neutral.shape);
    gs_effect_set_float(phase5->corner_param, neutral.corner_radius_px);
    gs_effect_set_float(filter->area_scale_param, neutral.area_scale);
    gs_effect_set_float(filter->circle_param, neutral.circle);
    return true;
}

void phase21_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter || !filter->phase5)
        return;

    /* True idle remains the accepted OBS skip/pass-through route.  Prime the
     * P5 ABI only when this frame is about to need the shared Draw technique. */
    if (phase21_presentation_required(filter)) {
        if (!phase21_prime_neutral_spotlight_abi(filter)) {
            static std::atomic<bool> warned{false};
            if (!warned.exchange(true, std::memory_order_acq_rel)) {
                blog(LOG_ERROR,
                     "[ArZoom][P0TRACE] Spotlight shader ABI incomplete; "
                     "refusing optional P5 state until all parameters exist");
            }
        }
    }

    /* phase18_render remains the sole owner of camera/click/cursor rendering.
     * If Spotlight is active it overwrites the neutral values with the real
     * mask uniforms before process_filter_end(). */
    phase18_render(filter, effect);
}

struct Phase21SourceInfoOverride {
    Phase21SourceInfoOverride()
    {
        arzoom_filter_info.video_render = phase21_render;
    }
};

Phase21SourceInfoOverride phase21_source_info_override;

} // namespace
