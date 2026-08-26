#pragma once

#include "arzoom-scene-camera-core.hpp"

#include <cmath>
#include <cstdint>

namespace arzoom {

struct SceneDisplayGeometrySnapshot {
    SceneMappingQuad quad{};
    float canvas_width = 0.0f;
    float canvas_height = 0.0f;
    float source_width = 0.0f;
    float source_height = 0.0f;
    float crop_left = 0.0f;
    float crop_top = 0.0f;
    float crop_right = 0.0f;
    float crop_bottom = 0.0f;
};

inline SceneMappingBuildResult scene_mapping_build_display_geometry(
    const SceneDisplayGeometrySnapshot &geometry)
{
    return scene_mapping_build_axis_aligned(
        geometry.quad,
        geometry.canvas_width,
        geometry.canvas_height,
        geometry.source_width,
        geometry.source_height,
        geometry.crop_left,
        geometry.crop_top,
        geometry.crop_right,
        geometry.crop_bottom);
}

struct SceneDesktopRect {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;

    bool valid() const
    {
        return right > left && bottom > top;
    }
};

inline bool scene_mapping_build_synthetic_desktop_rect(
    const SceneDesktopRect &physical,
    const SceneAxisAlignedMapping &mapping,
    std::int64_t coordinate_min,
    std::int64_t coordinate_max,
    SceneDesktopRect &mapped)
{
    if (!physical.valid() || !mapping.valid() ||
        coordinate_max <= coordinate_min) {
        return false;
    }

    const double scale_x = static_cast<double>(mapping.scene_scale.x);
    const double scale_y = static_cast<double>(mapping.scene_scale.y);
    const double physical_width =
        static_cast<double>(physical.right) - static_cast<double>(physical.left);
    const double physical_height =
        static_cast<double>(physical.bottom) - static_cast<double>(physical.top);
    const double width = physical_width / scale_x;
    const double height = physical_height / scale_y;
    const double left = static_cast<double>(physical.left) -
                        static_cast<double>(mapping.scene_offset.x) * width;
    const double top = static_cast<double>(physical.top) -
                       static_cast<double>(mapping.scene_offset.y) * height;
    const double right = left + width;
    const double bottom = top + height;

    const double lo = static_cast<double>(coordinate_min);
    const double hi = static_cast<double>(coordinate_max);
    if (!std::isfinite(left) || !std::isfinite(top) ||
        !std::isfinite(right) || !std::isfinite(bottom) ||
        left < lo || top < lo || right > hi || bottom > hi ||
        right - left < 2.0 || bottom - top < 2.0) {
        return false;
    }

    mapped = {
        static_cast<std::int64_t>(std::llround(left)),
        static_cast<std::int64_t>(std::llround(top)),
        static_cast<std::int64_t>(std::llround(right)),
        static_cast<std::int64_t>(std::llround(bottom)),
    };
    return mapped.valid();
}

} // namespace arzoom
