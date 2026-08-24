#pragma once

#include "arzoom-math.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace arzoom {

inline constexpr std::string_view kSceneCameraFilterId = "arzoom_filter";
inline constexpr std::string_view kSceneCameraFilterName = "ArZoom Camera";

enum class SceneCameraToggleAction {
    CreateEnabled,
    EnableExisting,
    DisableExisting,
};

inline bool scene_camera_filter_matches(std::string_view id,
                                        std::string_view name)
{
    return id == kSceneCameraFilterId && name == kSceneCameraFilterName;
}

inline SceneCameraToggleAction scene_camera_toggle_action(bool exists,
                                                           bool enabled)
{
    if (!exists)
        return SceneCameraToggleAction::CreateEnabled;
    return enabled ? SceneCameraToggleAction::DisableExisting
                   : SceneCameraToggleAction::EnableExisting;
}

struct SceneMappingQuad {
    Vec2 top_left{};
    Vec2 top_right{};
    Vec2 bottom_left{};
    Vec2 bottom_right{};
};

inline bool scene_mapping_is_full_canvas(const SceneMappingQuad &quad,
                                         float width,
                                         float height,
                                         float tolerance_px = 1.5f)
{
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0f || height <= 0.0f ||
        !std::isfinite(tolerance_px) || tolerance_px < 0.0f)
        return false;

    const auto close = [tolerance_px](Vec2 a, Vec2 b) {
        return std::fabs(a.x - b.x) <= tolerance_px &&
               std::fabs(a.y - b.y) <= tolerance_px;
    };

    return close(quad.top_left, {0.0f, 0.0f}) &&
           close(quad.top_right, {width, 0.0f}) &&
           close(quad.bottom_left, {0.0f, height}) &&
           close(quad.bottom_right, {width, height});
}

} // namespace arzoom
