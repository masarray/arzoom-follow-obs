#pragma once

#include "arzoom-math.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace arzoom {

inline constexpr std::string_view kSceneCameraFilterId = "arzoom_filter";
inline constexpr std::string_view kSceneCameraFilterName = "ArZoom Camera";
inline constexpr std::string_view kSceneCameraManagedSetting =
    "arzoom_scene_camera_managed";

enum class SceneCameraToggleAction {
    CreateEnabled,
    EnableExisting,
    DisableExisting,
};

inline bool scene_camera_filter_matches(std::string_view id,
                                        std::string_view name,
                                        bool managed_marker)
{
    if (id != kSceneCameraFilterId)
        return false;
    return managed_marker || name == kSceneCameraFilterName;
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

enum class SceneMappingStatus {
    Ok,
    InvalidCanvas,
    InvalidSource,
    InvalidCrop,
    RotatedOrSkewed,
    FlippedOrDegenerate,
};

inline std::string_view scene_mapping_status_text(SceneMappingStatus status)
{
    switch (status) {
    case SceneMappingStatus::Ok: return "ok";
    case SceneMappingStatus::InvalidCanvas: return "invalid scene canvas";
    case SceneMappingStatus::InvalidSource: return "invalid Display Capture size";
    case SceneMappingStatus::InvalidCrop: return "invalid/exhaustive scene-item crop";
    case SceneMappingStatus::RotatedOrSkewed: return "rotation/skew is not supported yet";
    case SceneMappingStatus::FlippedOrDegenerate: return "flipped/degenerate transform is not supported";
    }
    return "unknown mapping error";
}

struct SceneAxisAlignedMapping {
    Vec2 source_visible_min{0.0f, 0.0f};
    Vec2 source_visible_max{1.0f, 1.0f};

    /* scene_uv = scene_offset + source_uv * scene_scale */
    Vec2 scene_offset{0.0f, 0.0f};
    Vec2 scene_scale{1.0f, 1.0f};

    bool valid() const
    {
        return std::isfinite(scene_offset.x) && std::isfinite(scene_offset.y) &&
               std::isfinite(scene_scale.x) && std::isfinite(scene_scale.y) &&
               scene_scale.x > 0.0f && scene_scale.y > 0.0f &&
               source_visible_min.x >= 0.0f && source_visible_min.y >= 0.0f &&
               source_visible_max.x <= 1.0f && source_visible_max.y <= 1.0f &&
               source_visible_max.x > source_visible_min.x &&
               source_visible_max.y > source_visible_min.y;
    }
};

struct SceneMappingBuildResult {
    SceneMappingStatus status = SceneMappingStatus::InvalidCanvas;
    SceneAxisAlignedMapping mapping{};

    bool ok() const { return status == SceneMappingStatus::Ok && mapping.valid(); }
};

inline SceneMappingBuildResult scene_mapping_build_axis_aligned(
    const SceneMappingQuad &quad,
    float canvas_width,
    float canvas_height,
    float source_width,
    float source_height,
    float crop_left = 0.0f,
    float crop_top = 0.0f,
    float crop_right = 0.0f,
    float crop_bottom = 0.0f,
    float tolerance_px = 1.75f)
{
    SceneMappingBuildResult result;

    const auto finite_positive = [](float v) {
        return std::isfinite(v) && v > 0.0f;
    };
    if (!finite_positive(canvas_width) || !finite_positive(canvas_height)) {
        result.status = SceneMappingStatus::InvalidCanvas;
        return result;
    }
    if (!finite_positive(source_width) || !finite_positive(source_height)) {
        result.status = SceneMappingStatus::InvalidSource;
        return result;
    }
    if (!std::isfinite(crop_left) || !std::isfinite(crop_top) ||
        !std::isfinite(crop_right) || !std::isfinite(crop_bottom) ||
        crop_left < 0.0f || crop_top < 0.0f ||
        crop_right < 0.0f || crop_bottom < 0.0f ||
        crop_left + crop_right >= source_width ||
        crop_top + crop_bottom >= source_height) {
        result.status = SceneMappingStatus::InvalidCrop;
        return result;
    }

    const auto finite_point = [](Vec2 p) {
        return std::isfinite(p.x) && std::isfinite(p.y);
    };
    if (!finite_point(quad.top_left) || !finite_point(quad.top_right) ||
        !finite_point(quad.bottom_left) || !finite_point(quad.bottom_right)) {
        result.status = SceneMappingStatus::RotatedOrSkewed;
        return result;
    }

    const float tol = std::max(tolerance_px, 0.0f);
    const bool axis_aligned =
        std::fabs(quad.top_left.y - quad.top_right.y) <= tol &&
        std::fabs(quad.bottom_left.y - quad.bottom_right.y) <= tol &&
        std::fabs(quad.top_left.x - quad.bottom_left.x) <= tol &&
        std::fabs(quad.top_right.x - quad.bottom_right.x) <= tol;
    if (!axis_aligned) {
        result.status = SceneMappingStatus::RotatedOrSkewed;
        return result;
    }

    const float box_width = quad.top_right.x - quad.top_left.x;
    const float box_height = quad.bottom_left.y - quad.top_left.y;
    if (!finite_positive(box_width) || !finite_positive(box_height)) {
        result.status = SceneMappingStatus::FlippedOrDegenerate;
        return result;
    }

    SceneAxisAlignedMapping mapping;
    mapping.source_visible_min = {
        crop_left / source_width,
        crop_top / source_height,
    };
    mapping.source_visible_max = {
        1.0f - crop_right / source_width,
        1.0f - crop_bottom / source_height,
    };

    const Vec2 visible_span{
        mapping.source_visible_max.x - mapping.source_visible_min.x,
        mapping.source_visible_max.y - mapping.source_visible_min.y,
    };
    const Vec2 scene_box_min{
        quad.top_left.x / canvas_width,
        quad.top_left.y / canvas_height,
    };
    const Vec2 scene_box_span{
        box_width / canvas_width,
        box_height / canvas_height,
    };

    mapping.scene_scale = {
        scene_box_span.x / visible_span.x,
        scene_box_span.y / visible_span.y,
    };
    mapping.scene_offset = {
        scene_box_min.x - mapping.scene_scale.x * mapping.source_visible_min.x,
        scene_box_min.y - mapping.scene_scale.y * mapping.source_visible_min.y,
    };

    if (!mapping.valid()) {
        result.status = SceneMappingStatus::FlippedOrDegenerate;
        return result;
    }

    result.status = SceneMappingStatus::Ok;
    result.mapping = mapping;
    return result;
}

inline Vec2 scene_mapping_source_to_scene(
    const SceneAxisAlignedMapping &mapping, Vec2 source_uv)
{
    return {
        mapping.scene_offset.x + source_uv.x * mapping.scene_scale.x,
        mapping.scene_offset.y + source_uv.y * mapping.scene_scale.y,
    };
}

inline bool scene_mapping_source_visible(
    const SceneAxisAlignedMapping &mapping,
    Vec2 source_uv,
    float tolerance = 1.0e-5f)
{
    if (!mapping.valid() || !std::isfinite(source_uv.x) || !std::isfinite(source_uv.y))
        return false;
    return source_uv.x >= mapping.source_visible_min.x - tolerance &&
           source_uv.x <= mapping.source_visible_max.x + tolerance &&
           source_uv.y >= mapping.source_visible_min.y - tolerance &&
           source_uv.y <= mapping.source_visible_max.y + tolerance;
}

inline bool scene_mapping_scene_visible(Vec2 scene_uv,
                                        float tolerance = 1.0e-5f)
{
    return std::isfinite(scene_uv.x) && std::isfinite(scene_uv.y) &&
           scene_uv.x >= -tolerance && scene_uv.x <= 1.0f + tolerance &&
           scene_uv.y >= -tolerance && scene_uv.y <= 1.0f + tolerance;
}

} // namespace arzoom
