#include "../src/arzoom-scene-camera-core.hpp"
#include "../src/arzoom-presentation-cursor.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
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

bool near(arzoom::Vec2 a, arzoom::Vec2 b, float epsilon = 1.0e-5f)
{
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

void fullscreen_remains_identity()
{
    using namespace arzoom;
    const SceneMappingQuad quad{
        {0.0f, 0.0f}, {1920.0f, 0.0f},
        {0.0f, 1080.0f}, {1920.0f, 1080.0f}};
    const auto result = scene_mapping_build_axis_aligned(
        quad, 1920.0f, 1080.0f, 1920.0f, 1080.0f);
    require(result.ok(), "fullscreen P4 mapping stopped being valid");
    require(near(result.mapping.scene_offset, {0.0f, 0.0f}),
            "fullscreen mapping gained an offset");
    require(near(result.mapping.scene_scale, {1.0f, 1.0f}),
            "fullscreen mapping stopped being identity scale");
    require(near(scene_mapping_source_to_scene(
                     result.mapping, {0.25f, 0.75f}),
                 {0.25f, 0.75f}),
            "fullscreen source->scene mapping changed coordinates");
}

void inset_scaled_capture_maps_to_scene()
{
    using namespace arzoom;
    const SceneMappingQuad quad{
        {240.0f, 135.0f}, {1680.0f, 135.0f},
        {240.0f, 945.0f}, {1680.0f, 945.0f}};
    const auto result = scene_mapping_build_axis_aligned(
        quad, 1920.0f, 1080.0f, 1920.0f, 1080.0f);
    require(result.ok(), "axis-aligned inset Display Capture was rejected");

    require(near(scene_mapping_source_to_scene(result.mapping, {0.0f, 0.0f}),
                 {0.125f, 0.125f}),
            "inset top-left mapped incorrectly");
    require(near(scene_mapping_source_to_scene(result.mapping, {1.0f, 1.0f}),
                 {0.875f, 0.875f}),
            "inset bottom-right mapped incorrectly");
    require(near(scene_mapping_source_to_scene(result.mapping, {0.5f, 0.5f}),
                 {0.5f, 0.5f}),
            "inset center did not remain centered");
}

void asymmetric_inset_maps_exactly()
{
    using namespace arzoom;
    const SceneMappingQuad quad{
        {100.0f, 80.0f}, {1060.0f, 80.0f},
        {100.0f, 620.0f}, {1060.0f, 620.0f}};
    const auto result = scene_mapping_build_axis_aligned(
        quad, 1920.0f, 1080.0f, 2560.0f, 1440.0f);
    require(result.ok(), "asymmetric scaled capture was rejected");

    const auto mapped = scene_mapping_source_to_scene(
        result.mapping, {0.25f, 0.75f});
    require(near(mapped.x, (100.0f + 0.25f * 960.0f) / 1920.0f),
            "asymmetric x mapping incorrect");
    require(near(mapped.y, (80.0f + 0.75f * 540.0f) / 1080.0f),
            "asymmetric y mapping incorrect");
}

void crop_maps_only_visible_source_region()
{
    using namespace arzoom;
    const SceneMappingQuad quad{
        {200.0f, 100.0f}, {1720.0f, 100.0f},
        {200.0f, 980.0f}, {1720.0f, 980.0f}};
    const auto result = scene_mapping_build_axis_aligned(
        quad, 1920.0f, 1080.0f, 1920.0f, 1080.0f,
        192.0f, 108.0f, 384.0f, 216.0f);
    require(result.ok(), "valid crop-aware mapping was rejected");

    const arzoom::Vec2 min_uv{0.10f, 0.10f};
    const arzoom::Vec2 max_uv{0.80f, 0.80f};
    require(scene_mapping_source_visible(result.mapping, min_uv),
            "visible crop minimum was rejected");
    require(scene_mapping_source_visible(result.mapping, max_uv),
            "visible crop maximum was rejected");
    require(!scene_mapping_source_visible(result.mapping, {0.05f, 0.5f}),
            "cropped-away left region remained pointer-active");
    require(!scene_mapping_source_visible(result.mapping, {0.9f, 0.5f}),
            "cropped-away right region remained pointer-active");

    require(near(scene_mapping_source_to_scene(result.mapping, min_uv),
                 {200.0f / 1920.0f, 100.0f / 1080.0f}, 2.0e-5f),
            "visible crop minimum did not map to box top-left");
    require(near(scene_mapping_source_to_scene(result.mapping, max_uv),
                 {1720.0f / 1920.0f, 980.0f / 1080.0f}, 2.0e-5f),
            "visible crop maximum did not map to box bottom-right");
}

void unsupported_transforms_fail_safe()
{
    using namespace arzoom;
    const SceneMappingQuad rotated{
        {100.0f, 80.0f}, {1050.0f, 130.0f},
        {60.0f, 620.0f}, {1010.0f, 670.0f}};
    require(scene_mapping_build_axis_aligned(
                rotated, 1920.0f, 1080.0f, 1920.0f, 1080.0f).status ==
                SceneMappingStatus::RotatedOrSkewed,
            "rotated capture did not fail safe");

    const SceneMappingQuad flipped{
        {1000.0f, 80.0f}, {100.0f, 80.0f},
        {1000.0f, 620.0f}, {100.0f, 620.0f}};
    require(scene_mapping_build_axis_aligned(
                flipped, 1920.0f, 1080.0f, 1920.0f, 1080.0f).status ==
                SceneMappingStatus::FlippedOrDegenerate,
            "flipped capture did not fail safe");

    const SceneMappingQuad normal{
        {100.0f, 80.0f}, {1000.0f, 80.0f},
        {100.0f, 620.0f}, {1000.0f, 620.0f}};
    require(scene_mapping_build_axis_aligned(
                normal, 1920.0f, 1080.0f, 1920.0f, 1080.0f,
                1000.0f, 0.0f, 920.0f, 0.0f).status ==
                SceneMappingStatus::InvalidCrop,
            "exhaustive crop did not fail safe");
}

void scene_visibility_gate_rejects_off_canvas_mapping()
{
    using namespace arzoom;
    const SceneMappingQuad quad{
        {-300.0f, 100.0f}, {900.0f, 100.0f},
        {-300.0f, 775.0f}, {900.0f, 775.0f}};
    const auto result = scene_mapping_build_axis_aligned(
        quad, 1920.0f, 1080.0f, 1920.0f, 1080.0f);
    require(result.ok(), "partially off-canvas axis-aligned mapping was rejected");
    require(!scene_mapping_scene_visible(
                scene_mapping_source_to_scene(result.mapping, {0.0f, 0.5f})),
            "off-canvas pointer was considered scene-visible");
    require(scene_mapping_scene_visible(
                scene_mapping_source_to_scene(result.mapping, {0.5f, 0.5f})),
            "visible portion of off-canvas capture was rejected");
}

void pointer_size_follows_increase_and_decrease_zoom()
{
    using namespace arzoom;
    const float base = 52.0f;
    const float zooms[] = {1.0f, 1.25f, 2.0f, 3.0f, 4.0f, 3.0f, 2.0f, 1.25f, 1.0f};
    float previous = presentation_cursor_scaled_height(base, zooms[0]);
    for (size_t i = 1; i < sizeof(zooms) / sizeof(zooms[0]); ++i) {
        const float current = presentation_cursor_scaled_height(base, zooms[i]);
        if (zooms[i] > zooms[i - 1])
            require(current > previous, "Zoom In did not enlarge Presentation Cursor");
        else if (zooms[i] < zooms[i - 1])
            require(current < previous, "Zoom Out did not shrink Presentation Cursor");
        previous = current;
    }
    require(near(presentation_cursor_scaled_height(base, 1.0f), base),
            "1x cursor did not return to configured base size");
}

} // namespace

int main()
{
    fullscreen_remains_identity();
    inset_scaled_capture_maps_to_scene();
    asymmetric_inset_maps_exactly();
    crop_maps_only_visible_source_region();
    unsupported_transforms_fail_safe();
    scene_visibility_gate_rejects_off_canvas_mapping();
    pointer_size_follows_increase_and_decrease_zoom();
    std::cout << "ArZoom Phase 4.1 scene mapping + cursor scale gates: PASS\n";
    return 0;
}
