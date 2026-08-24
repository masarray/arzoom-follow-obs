#pragma once

namespace arzoom {

/*
 * OBS effects keep texture parameters across draws.  The Presentation Cursor
 * sampler is part of the same one-pass effect used by click visualization, so
 * the effect must never enter that pass with an unbound cursor texture even
 * when cursor_visible is zero.  Some graphics backends may still validate or
 * speculatively touch declared samplers before the pixel-shader branch exits.
 *
 * Keep the policy pure and testable: only a fully usable cursor runtime may
 * rely on the real atlas; every other state must bind a transparent fallback.
 */
inline bool cursor_requires_transparent_fallback(bool cursor_enabled,
                                                 bool position_valid,
                                                 bool shader_ready,
                                                 bool atlas_ready)
{
    return !(cursor_enabled && position_valid && shader_ready && atlas_ready);
}

} // namespace arzoom
