#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

namespace arzoom {

inline constexpr std::size_t kMultiMaxPresentationScreens = 8;
inline constexpr std::size_t kMultiInvalidScreenIndex =
    std::numeric_limits<std::size_t>::max();

struct MultiPoint {
    double x = 0.0;
    double y = 0.0;
};

struct MultiPhysicalRect {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;

    constexpr bool valid() const
    {
        return right > left && bottom > top;
    }
};

struct MultiCrop {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

struct MultiSceneRect {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

/* Full-source pixel -> scene pixel affine transform.
 *
 * scene_x = xx * source_x + xy * source_y + tx
 * scene_y = yx * source_x + yy * source_y + ty
 *
 * M1 deliberately accepts only positive axis-aligned transforms. Keeping the
 * canonical affine representation now lets a later milestone expand support
 * without changing the published state shape. */
struct MultiAffine2D {
    double xx = 1.0;
    double xy = 0.0;
    double yx = 0.0;
    double yy = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

enum class MultiGeometryStatus : std::uint8_t {
    Ready,
    InvalidPhysicalRect,
    InvalidSourceSize,
    InvalidCrop,
    InvalidSceneRect,
    UnsupportedTransform,
};

inline constexpr std::string_view multi_geometry_status_text(
    MultiGeometryStatus status)
{
    switch (status) {
    case MultiGeometryStatus::Ready: return "ready";
    case MultiGeometryStatus::InvalidPhysicalRect:
        return "invalid physical monitor rectangle";
    case MultiGeometryStatus::InvalidSourceSize:
        return "invalid source size";
    case MultiGeometryStatus::InvalidCrop: return "invalid source crop";
    case MultiGeometryStatus::InvalidSceneRect:
        return "invalid scene rectangle";
    case MultiGeometryStatus::UnsupportedTransform:
        return "unsupported scene transform";
    }
    return "unknown geometry status";
}

struct MultiPresentationScreen {
    bool eligible = false;
    MultiPhysicalRect physical{};
    double source_width = 0.0;
    double source_height = 0.0;
    MultiCrop crop{};
    MultiAffine2D source_to_scene{};
};

enum class MultiPointerStatus : std::uint8_t {
    Ready,
    NoEligibleScreens,
    CursorOutside,
    AmbiguousPhysicalOwner,
    UnsupportedGeometry,
    CursorOutsideVisibleSource,
    TooManyScreens,
};

inline constexpr std::string_view multi_pointer_status_text(
    MultiPointerStatus status)
{
    switch (status) {
    case MultiPointerStatus::Ready: return "ready";
    case MultiPointerStatus::NoEligibleScreens: return "no eligible screens";
    case MultiPointerStatus::CursorOutside:
        return "cursor outside presentation screens";
    case MultiPointerStatus::AmbiguousPhysicalOwner:
        return "ambiguous physical display ownership";
    case MultiPointerStatus::UnsupportedGeometry:
        return "presentation screen geometry unavailable";
    case MultiPointerStatus::CursorOutsideVisibleSource:
        return "cursor outside visible cropped source";
    case MultiPointerStatus::TooManyScreens:
        return "presentation screen limit exceeded";
    }
    return "unknown pointer status";
}

struct MultiScenePointer {
    MultiPointerStatus status = MultiPointerStatus::NoEligibleScreens;
    MultiGeometryStatus geometry_status = MultiGeometryStatus::Ready;
    std::size_t active_screen = kMultiInvalidScreenIndex;
    double source_u = 0.0;
    double source_v = 0.0;
    double scene_x = 0.0;
    double scene_y = 0.0;

    constexpr bool ready() const
    {
        return status == MultiPointerStatus::Ready &&
               active_screen != kMultiInvalidScreenIndex;
    }
};

struct MultiTransformBuildResult {
    MultiGeometryStatus status = MultiGeometryStatus::InvalidSceneRect;
    MultiAffine2D transform{};

    constexpr bool ready() const
    {
        return status == MultiGeometryStatus::Ready;
    }
};

inline bool multi_finite(double value)
{
    return std::isfinite(value);
}

inline bool multi_scene_rect_valid(const MultiSceneRect &rect)
{
    return multi_finite(rect.left) && multi_finite(rect.top) &&
           multi_finite(rect.right) && multi_finite(rect.bottom) &&
           rect.right > rect.left && rect.bottom > rect.top;
}

inline bool multi_source_size_valid(double width, double height)
{
    return multi_finite(width) && multi_finite(height) &&
           width > 0.0 && height > 0.0;
}

inline bool multi_crop_valid(const MultiCrop &crop,
                             double source_width,
                             double source_height)
{
    return multi_source_size_valid(source_width, source_height) &&
           multi_finite(crop.left) && multi_finite(crop.top) &&
           multi_finite(crop.right) && multi_finite(crop.bottom) &&
           crop.left >= 0.0 && crop.top >= 0.0 &&
           crop.right >= 0.0 && crop.bottom >= 0.0 &&
           crop.left + crop.right < source_width &&
           crop.top + crop.bottom < source_height;
}

inline bool multi_affine_supported_axis_aligned(
    const MultiAffine2D &transform,
    double zero_tolerance = 1.0e-9)
{
    if (!multi_finite(transform.xx) || !multi_finite(transform.xy) ||
        !multi_finite(transform.yx) || !multi_finite(transform.yy) ||
        !multi_finite(transform.tx) || !multi_finite(transform.ty) ||
        !multi_finite(zero_tolerance) || zero_tolerance < 0.0) {
        return false;
    }

    return std::fabs(transform.xy) <= zero_tolerance &&
           std::fabs(transform.yx) <= zero_tolerance &&
           transform.xx > 0.0 && transform.yy > 0.0;
}

inline MultiTransformBuildResult multi_build_axis_aligned_transform(
    double source_width,
    double source_height,
    const MultiCrop &crop,
    const MultiSceneRect &scene_rect)
{
    MultiTransformBuildResult result;

    if (!multi_source_size_valid(source_width, source_height)) {
        result.status = MultiGeometryStatus::InvalidSourceSize;
        return result;
    }
    if (!multi_crop_valid(crop, source_width, source_height)) {
        result.status = MultiGeometryStatus::InvalidCrop;
        return result;
    }
    if (!multi_scene_rect_valid(scene_rect)) {
        result.status = MultiGeometryStatus::InvalidSceneRect;
        return result;
    }

    const double visible_width = source_width - crop.left - crop.right;
    const double visible_height = source_height - crop.top - crop.bottom;
    const double scale_x =
        (scene_rect.right - scene_rect.left) / visible_width;
    const double scale_y =
        (scene_rect.bottom - scene_rect.top) / visible_height;

    MultiAffine2D transform;
    transform.xx = scale_x;
    transform.yy = scale_y;
    transform.tx = scene_rect.left - scale_x * crop.left;
    transform.ty = scene_rect.top - scale_y * crop.top;

    if (!multi_affine_supported_axis_aligned(transform)) {
        result.status = MultiGeometryStatus::UnsupportedTransform;
        return result;
    }

    result.status = MultiGeometryStatus::Ready;
    result.transform = transform;
    return result;
}

inline MultiGeometryStatus multi_screen_geometry_status(
    const MultiPresentationScreen &screen)
{
    if (!screen.physical.valid())
        return MultiGeometryStatus::InvalidPhysicalRect;
    if (!multi_source_size_valid(screen.source_width, screen.source_height))
        return MultiGeometryStatus::InvalidSourceSize;
    if (!multi_crop_valid(screen.crop, screen.source_width,
                          screen.source_height))
        return MultiGeometryStatus::InvalidCrop;
    if (!multi_affine_supported_axis_aligned(screen.source_to_scene))
        return MultiGeometryStatus::UnsupportedTransform;
    return MultiGeometryStatus::Ready;
}

inline bool multi_physical_contains(const MultiPhysicalRect &rect,
                                    std::int64_t x,
                                    std::int64_t y)
{
    return rect.valid() && x >= rect.left && x < rect.right &&
           y >= rect.top && y < rect.bottom;
}

inline MultiPoint multi_physical_to_source_uv(const MultiPhysicalRect &rect,
                                              std::int64_t x,
                                              std::int64_t y)
{
    if (!multi_physical_contains(rect, x, y))
        return {-1.0, -1.0};

    const long double width = static_cast<long double>(rect.right) -
                              static_cast<long double>(rect.left);
    const long double height = static_cast<long double>(rect.bottom) -
                               static_cast<long double>(rect.top);
    const long double local_x = static_cast<long double>(x) -
                                static_cast<long double>(rect.left);
    const long double local_y = static_cast<long double>(y) -
                                static_cast<long double>(rect.top);

    return {
        static_cast<double>(local_x / width),
        static_cast<double>(local_y / height),
    };
}

inline MultiPoint multi_source_uv_to_pixels(double source_width,
                                            double source_height,
                                            MultiPoint uv)
{
    return {uv.x * source_width, uv.y * source_height};
}

inline bool multi_source_point_visible(const MultiPresentationScreen &screen,
                                       MultiPoint source_point)
{
    if (!multi_crop_valid(screen.crop, screen.source_width,
                          screen.source_height) ||
        !multi_finite(source_point.x) || !multi_finite(source_point.y)) {
        return false;
    }

    const double visible_right = screen.source_width - screen.crop.right;
    const double visible_bottom = screen.source_height - screen.crop.bottom;
    return source_point.x >= screen.crop.left &&
           source_point.x < visible_right &&
           source_point.y >= screen.crop.top &&
           source_point.y < visible_bottom;
}

inline MultiPoint multi_apply_affine(const MultiAffine2D &transform,
                                     MultiPoint point)
{
    return {
        transform.xx * point.x + transform.xy * point.y + transform.tx,
        transform.yx * point.x + transform.yy * point.y + transform.ty,
    };
}

inline MultiScenePointer multi_resolve_scene_pointer(
    const MultiPresentationScreen *screens,
    std::size_t count,
    std::int64_t cursor_x,
    std::int64_t cursor_y)
{
    MultiScenePointer result;

    if (count > kMultiMaxPresentationScreens) {
        result.status = MultiPointerStatus::TooManyScreens;
        return result;
    }
    if (count == 0) {
        result.status = MultiPointerStatus::NoEligibleScreens;
        return result;
    }
    if (!screens) {
        result.status = MultiPointerStatus::UnsupportedGeometry;
        result.geometry_status = MultiGeometryStatus::InvalidPhysicalRect;
        return result;
    }

    std::size_t eligible_count = 0;
    std::size_t invalid_physical_count = 0;
    std::size_t owner_count = 0;
    std::size_t owner_index = kMultiInvalidScreenIndex;

    for (std::size_t i = 0; i < count; ++i) {
        const MultiPresentationScreen &screen = screens[i];
        if (!screen.eligible)
            continue;

        ++eligible_count;
        if (!screen.physical.valid()) {
            ++invalid_physical_count;
            continue;
        }
        if (!multi_physical_contains(screen.physical, cursor_x, cursor_y))
            continue;

        ++owner_count;
        owner_index = i;
    }

    if (eligible_count == 0) {
        result.status = MultiPointerStatus::NoEligibleScreens;
        return result;
    }
    if (owner_count > 1) {
        result.status = MultiPointerStatus::AmbiguousPhysicalOwner;
        return result;
    }
    if (owner_count == 0) {
        if (invalid_physical_count > 0) {
            result.status = MultiPointerStatus::UnsupportedGeometry;
            result.geometry_status = MultiGeometryStatus::InvalidPhysicalRect;
        } else {
            result.status = MultiPointerStatus::CursorOutside;
        }
        return result;
    }

    const MultiPresentationScreen &owner = screens[owner_index];
    result.active_screen = owner_index;
    result.geometry_status = multi_screen_geometry_status(owner);
    if (result.geometry_status != MultiGeometryStatus::Ready) {
        result.status = MultiPointerStatus::UnsupportedGeometry;
        return result;
    }

    const MultiPoint uv = multi_physical_to_source_uv(
        owner.physical, cursor_x, cursor_y);
    const MultiPoint source_point = multi_source_uv_to_pixels(
        owner.source_width, owner.source_height, uv);

    result.source_u = uv.x;
    result.source_v = uv.y;

    if (!multi_source_point_visible(owner, source_point)) {
        result.status = MultiPointerStatus::CursorOutsideVisibleSource;
        return result;
    }

    const MultiPoint scene_point = multi_apply_affine(
        owner.source_to_scene, source_point);
    if (!multi_finite(scene_point.x) || !multi_finite(scene_point.y)) {
        result.status = MultiPointerStatus::UnsupportedGeometry;
        result.geometry_status = MultiGeometryStatus::UnsupportedTransform;
        return result;
    }

    result.scene_x = scene_point.x;
    result.scene_y = scene_point.y;
    result.status = MultiPointerStatus::Ready;
    return result;
}

static_assert(std::is_trivially_copyable<MultiPresentationScreen>::value,
              "M1 prepared screen must remain value-only/trivially copyable");
static_assert(std::is_trivially_copyable<MultiScenePointer>::value,
              "M1 pointer snapshot must remain value-only/trivially copyable");

} // namespace arzoom
