#include "../src/arzoom-scene-mapping-runtime.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

bool near(float a, float b, float epsilon = 1.0e-5f)
{
    return std::fabs(a - b) <= epsilon;
}

constexpr std::int64_t kCoordMin =
    static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::lowest());
constexpr std::int64_t kCoordMax =
    static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());

void geometry_seam_matches_p41_mapping_contract()
{
    using namespace arzoom;
    const SceneDisplayGeometrySnapshot geometry{
        {{200.0f, 100.0f}, {1720.0f, 100.0f},
         {200.0f, 980.0f}, {1720.0f, 980.0f}},
        1920.0f, 1080.0f, 1920.0f, 1080.0f,
        192.0f, 108.0f, 384.0f, 216.0f};

    const auto via_seam = scene_mapping_build_display_geometry(geometry);
    const auto direct = scene_mapping_build_axis_aligned(
        geometry.quad,
        geometry.canvas_width,
        geometry.canvas_height,
        geometry.source_width,
        geometry.source_height,
        geometry.crop_left,
        geometry.crop_top,
        geometry.crop_right,
        geometry.crop_bottom);

    require(via_seam.status == direct.status && via_seam.ok() == direct.ok(),
            "extracted geometry seam changed P4.1 mapping status");
    require(near(via_seam.mapping.scene_offset.x,
                 direct.mapping.scene_offset.x) &&
                near(via_seam.mapping.scene_offset.y,
                     direct.mapping.scene_offset.y) &&
                near(via_seam.mapping.scene_scale.x,
                     direct.mapping.scene_scale.x) &&
                near(via_seam.mapping.scene_scale.y,
                     direct.mapping.scene_scale.y),
            "extracted geometry seam changed P4.1 affine mapping");
}

void fullscreen_synthetic_rect_remains_identity()
{
    using namespace arzoom;
    const SceneDisplayGeometrySnapshot geometry{
        {{0.0f, 0.0f}, {1920.0f, 0.0f},
         {0.0f, 1080.0f}, {1920.0f, 1080.0f}},
        1920.0f, 1080.0f, 1920.0f, 1080.0f};
    const auto mapping = scene_mapping_build_display_geometry(geometry);
    require(mapping.ok(), "fullscreen geometry stopped being valid");

    SceneDesktopRect mapped;
    require(scene_mapping_build_synthetic_desktop_rect(
                {0, 0, 1920, 1080}, mapping.mapping,
                kCoordMin, kCoordMax, mapped),
            "fullscreen synthetic monitor build failed");
    require(mapped.left == 0 && mapped.top == 0 &&
                mapped.right == 1920 && mapped.bottom == 1080,
            "fullscreen synthetic monitor stopped matching physical monitor");
}

void inset_mapping_preserves_inherited_normalization()
{
    using namespace arzoom;
    const SceneDisplayGeometrySnapshot geometry{
        {{480.0f, 270.0f}, {1440.0f, 270.0f},
         {480.0f, 810.0f}, {1440.0f, 810.0f}},
        1920.0f, 1080.0f, 1920.0f, 1080.0f};
    const auto mapping = scene_mapping_build_display_geometry(geometry);
    require(mapping.ok(), "inset geometry stopped being valid");

    SceneDesktopRect mapped;
    require(scene_mapping_build_synthetic_desktop_rect(
                {0, 0, 1920, 1080}, mapping.mapping,
                kCoordMin, kCoordMax, mapped),
            "inset synthetic monitor build failed");
    require(mapped.left == -960 && mapped.top == -540 &&
                mapped.right == 2880 && mapped.bottom == 1620,
            "inset synthetic desktop rect changed P4.1 behavior");

    const float inherited_x =
        static_cast<float>(960 - mapped.left) /
        static_cast<float>(mapped.right - mapped.left);
    const auto expected = scene_mapping_source_to_scene(
        mapping.mapping, {0.5f, 0.5f});
    require(near(inherited_x, expected.x),
            "synthetic monitor normalization no longer equals scene mapping");
}

void negative_desktop_coordinates_remain_supported()
{
    using namespace arzoom;
    SceneAxisAlignedMapping mapping;
    mapping.scene_scale = {0.5f, 1.0f};

    SceneDesktopRect mapped;
    require(scene_mapping_build_synthetic_desktop_rect(
                {-1920, 0, 0, 1080}, mapping,
                kCoordMin, kCoordMax, mapped),
            "negative physical monitor failed synthetic mapping");
    require(mapped.left == -1920 && mapped.right == 1920,
            "negative virtual-desktop coordinates were not preserved");
}

void invalid_geometry_and_overflow_fail_safe()
{
    using namespace arzoom;
    const SceneDisplayGeometrySnapshot skewed{
        {{0.0f, 0.0f}, {100.0f, 50.0f},
         {0.0f, 100.0f}, {100.0f, 100.0f}},
        1920.0f, 1080.0f, 1920.0f, 1080.0f};
    require(!scene_mapping_build_display_geometry(skewed).ok(),
            "unsupported skew became valid through extracted seam");

    SceneAxisAlignedMapping tiny_scale;
    tiny_scale.scene_scale = {1.0e-12f, 1.0e-12f};
    SceneDesktopRect mapped;
    require(!scene_mapping_build_synthetic_desktop_rect(
                {0, 0, 1920, 1080}, tiny_scale,
                kCoordMin, kCoordMax, mapped),
            "unsafe synthetic desktop overflow was guessed instead of rejected");
}

} // namespace

int main()
{
    geometry_seam_matches_p41_mapping_contract();
    fullscreen_synthetic_rect_remains_identity();
    inset_mapping_preserves_inherited_normalization();
    negative_desktop_coordinates_remain_supported();
    invalid_geometry_and_overflow_fail_safe();
    std::cout << "ArZoom P4.2 behavior-neutral mapping seam gates: PASS\n";
    return 0;
}
