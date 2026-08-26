#pragma once

namespace arzoom {

enum class CursorSamplerRoute {
    TransparentFallback = 0,
    RealAtlas,
};

/*
 * Every ArZoom presentation draw declares cursor_atlas in the shared effect.
 * The sampler therefore needs a live texture before OBS enters the processed
 * filter path.  Do not rely on cursor_visible branches or bind the real atlas
 * only after obs_source_process_filter_begin().
 */
inline CursorSamplerRoute cursor_sampler_route(bool cursor_enabled,
                                               bool position_valid,
                                               bool shader_ready,
                                               bool atlas_ready)
{
    return cursor_enabled && position_valid && shader_ready && atlas_ready
               ? CursorSamplerRoute::RealAtlas
               : CursorSamplerRoute::TransparentFallback;
}

} // namespace arzoom
