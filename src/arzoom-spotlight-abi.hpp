#pragma once

namespace arzoom {

/*
 * D3D11 validates the complete parameter set used by a compiled technique.
 * Therefore Spotlight OFF cannot mean "only set spotlight_enabled = 0" when
 * the same Draw technique still references the rest of the Spotlight ABI.
 *
 * This packet gives every P5-only uniform a deterministic, visually neutral
 * value before any non-Spotlight camera/click/cursor processed draw.
 */
struct SpotlightNeutralAbi {
    float center_x = 0.5f;
    float center_y = 0.5f;
    float half_size_x_px = 1.0f;
    float half_size_y_px = 1.0f;
    float feather_px = 1.0f;
    float dim_strength = 0.0f;
    float shape = 0.0f;
    float corner_radius_px = 0.0f;
    float area_scale = 1.0f;
    float circle = 1.0f;
    float enabled = 0.0f;
};

inline constexpr SpotlightNeutralAbi spotlight_neutral_abi()
{
    return {};
}

} // namespace arzoom
