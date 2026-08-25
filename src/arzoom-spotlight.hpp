#pragma once

#include <algorithm>
#include <cmath>

namespace arzoom {

enum class SpotlightMode {
    SmartFocus,
    Cursor,
    Click,
};

enum class SpotlightSize {
    Compact,
    Balanced,
    Wide,
};

enum class SpotlightShape {
    Ellipse,
    RoundedRectangle,
};

struct SpotlightVec2 {
    float x = 0.5f;
    float y = 0.5f;
};

struct SpotlightGeometry {
    SpotlightVec2 center_output{0.5f, 0.45f};
    SpotlightVec2 half_size_px{260.0f, 170.0f};
    float feather_px = 92.0f;
    float corner_radius_px = 42.0f;
    float dim_strength = 0.38f;
    SpotlightShape shape = SpotlightShape::Ellipse;
};

inline SpotlightVec2 spotlight_half_size_px(SpotlightSize size,
                                             float viewport_width,
                                             float viewport_height)
{
    const float w = std::max(viewport_width, 1.0f);
    const float h = std::max(viewport_height, 1.0f);

    float width_fraction = 0.27f;
    float height_fraction = 0.30f;
    switch (size) {
    case SpotlightSize::Compact:
        width_fraction = 0.22f;
        height_fraction = 0.24f;
        break;
    case SpotlightSize::Wide:
        width_fraction = 0.40f;
        height_fraction = 0.38f;
        break;
    case SpotlightSize::Balanced:
    default:
        break;
    }

    return {
        std::max(96.0f, w * width_fraction * 0.5f),
        std::max(72.0f, h * height_fraction * 0.5f),
    };
}

inline float spotlight_ellipse_signed_distance_px(SpotlightVec2 delta_px,
                                                   SpotlightVec2 half_size_px)
{
    const float rx = std::max(half_size_px.x, 1.0f);
    const float ry = std::max(half_size_px.y, 1.0f);
    const float nx = delta_px.x / rx;
    const float ny = delta_px.y / ry;
    const float normalized = std::sqrt(nx * nx + ny * ny);
    return (normalized - 1.0f) * std::min(rx, ry);
}

inline float spotlight_rounded_rect_signed_distance_px(
    SpotlightVec2 delta_px, SpotlightVec2 half_size_px,
    float corner_radius_px)
{
    const float hx = std::max(half_size_px.x, 1.0f);
    const float hy = std::max(half_size_px.y, 1.0f);
    const float radius = std::clamp(corner_radius_px, 0.0f,
                                    std::min(hx, hy));
    const float qx = std::fabs(delta_px.x) - (hx - radius);
    const float qy = std::fabs(delta_px.y) - (hy - radius);
    const float ox = std::max(qx, 0.0f);
    const float oy = std::max(qy, 0.0f);
    const float outside = std::sqrt(ox * ox + oy * oy);
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return outside + inside - radius;
}

inline float spotlight_smoothstep(float edge0, float edge1, float x)
{
    if (edge1 <= edge0)
        return x >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float spotlight_focus_weight(float signed_distance_px,
                                    float feather_px)
{
    const float feather = std::max(feather_px, 1.0f);
    return 1.0f - spotlight_smoothstep(0.0f, feather,
                                       signed_distance_px);
}

inline float spotlight_scene_multiplier(float signed_distance_px,
                                        float feather_px,
                                        float dim_strength,
                                        bool enabled)
{
    if (!enabled)
        return 1.0f;
    const float dim = std::clamp(dim_strength, 0.0f, 0.75f);
    const float focus = spotlight_focus_weight(signed_distance_px, feather_px);
    return 1.0f - dim * (1.0f - focus);
}

} // namespace arzoom
