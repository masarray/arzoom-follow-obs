#include "arzoom-filter-v12.cpp"

/*
 * P4.1 cursor-size closeout
 * -------------------------
 * Keep pointer size ownership in the final active wrapper.  This prevents a
 * later settings/update wrapper from leaving the Presentation Cursor at its
 * base output-pixel size after Zoom +/- changes the live camera magnification.
 */
namespace {

void phase41_cursor_scaled_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter || !filter->phase4)
        return;

    prime_cursor_sampler_safety(filter->phase4);

    Phase351Filter *phase351 = phase351_from_phase41(filter);
    Phase35Filter *phase35 = phase35_from_phase4(filter->phase4);
    ArZoomFilter *phase1 = phase1_from_phase41(filter);
    if (phase351 && phase35 && phase1) {
        const float base_size =
            phase351->base_cursor_size_px.load(std::memory_order_acquire);
        const float live_zoom = std::max(phase1->current_zoom, 1.0f);
        const float scaled_size =
            arzoom::presentation_cursor_scaled_height(base_size, live_zoom);
        phase35->cursor_size_px.store(scaled_size, std::memory_order_release);

        /* phase35_render owns the complete shared GPU pass: camera transform,
         * click rings and Presentation Cursor.  Calling it directly here avoids
         * any lower wrapper reapplying the unscaled settings value. */
        phase35_render(phase35, effect);
        return;
    }

    phase41_render(filter, effect);
}

struct Phase41CursorScaleSourceInfoOverride {
    Phase41CursorScaleSourceInfoOverride()
    {
        arzoom_filter_info.video_render = phase41_cursor_scaled_render;
    }
};

Phase41CursorScaleSourceInfoOverride phase41_cursor_scale_source_info_override;

} // namespace
