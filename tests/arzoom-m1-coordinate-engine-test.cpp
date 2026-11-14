#include "../src/arzoom-multi-coordinate.hpp"

#include <cmath>
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

void require_near(double actual, double expected, double tolerance,
                  const std::string &message)
{
    require(std::fabs(actual - expected) <= tolerance,
            message + " actual=" + std::to_string(actual) +
                " expected=" + std::to_string(expected));
}

arzoom::MultiPresentationScreen make_screen(
    arzoom::MultiPhysicalRect physical,
    double source_width,
    double source_height,
    arzoom::MultiCrop crop,
    arzoom::MultiSceneRect scene_rect,
    bool eligible = true)
{
    const auto built = arzoom::multi_build_axis_aligned_transform(
        source_width, source_height, crop, scene_rect);
    require(built.ready(), "test fixture transform did not build");

    arzoom::MultiPresentationScreen screen;
    screen.eligible = eligible;
    screen.physical = physical;
    screen.source_width = source_width;
    screen.source_height = source_height;
    screen.crop = crop;
    screen.source_to_scene = built.transform;
    return screen;
}

void two_independently_scaled_side_by_side_sources_map_correctly()
{
    const arzoom::MultiPresentationScreen screens[] = {
        make_screen({0, 0, 1920, 1080}, 1920.0, 1080.0, {},
                    {80.0, 40.0, 880.0, 940.0}),
        make_screen({1920, 0, 3840, 1080}, 2560.0, 1440.0, {},
                    {1040.0, 120.0, 1880.0, 900.0}),
    };

    const auto a = arzoom::multi_resolve_scene_pointer(
        screens, 2, 960, 540);
    require(a.ready() && a.active_screen == 0,
            "Monitor A did not resolve to screen A");
    require_near(a.source_u, 0.5, 1.0e-12,
                 "Monitor A source U was not canonical");
    require_near(a.source_v, 0.5, 1.0e-12,
                 "Monitor A source V was not canonical");
    require_near(a.scene_x, 480.0, 1.0e-9,
                 "Monitor A did not land at its independently scaled scene center X");
    require_near(a.scene_y, 490.0, 1.0e-9,
                 "Monitor A did not land at its independently scaled scene center Y");

    const auto b = arzoom::multi_resolve_scene_pointer(
        screens, 2, 2880, 540);
    require(b.ready() && b.active_screen == 1,
            "Monitor B did not resolve to screen B");
    require_near(b.source_u, 0.5, 1.0e-12,
                 "Monitor B source U was not canonical");
    require_near(b.source_v, 0.5, 1.0e-12,
                 "Monitor B source V was not canonical");
    require_near(b.scene_x, 1460.0, 1.0e-9,
                 "Monitor B did not land at its independent scene center X");
    require_near(b.scene_y, 510.0, 1.0e-9,
                 "Monitor B did not land at its independent scene center Y");
}

void negative_coordinates_and_half_open_boundaries_are_deterministic()
{
    const arzoom::MultiPresentationScreen screens[] = {
        make_screen({-1920, -200, 0, 880}, 1920.0, 1080.0, {},
                    {0.0, 0.0, 960.0, 540.0}),
        make_screen({0, -200, 1920, 880}, 1920.0, 1080.0, {},
                    {960.0, 0.0, 1920.0, 540.0}),
    };

    const auto left_edge = arzoom::multi_resolve_scene_pointer(
        screens, 2, -1920, -200);
    require(left_edge.ready() && left_edge.active_screen == 0,
            "negative top-left edge was not included");
    require_near(left_edge.source_u, 0.0, 1.0e-12,
                 "negative edge U was not zero");
    require_near(left_edge.source_v, 0.0, 1.0e-12,
                 "negative edge V was not zero");

    const auto left_last = arzoom::multi_resolve_scene_pointer(
        screens, 2, -1, 0);
    require(left_last.ready() && left_last.active_screen == 0,
            "x=-1 did not remain owned by left monitor");

    const auto right_first = arzoom::multi_resolve_scene_pointer(
        screens, 2, 0, 0);
    require(right_first.ready() && right_first.active_screen == 1,
            "half-open x=0 boundary did not transfer to right monitor");

    const auto outside = arzoom::multi_resolve_scene_pointer(
        screens, 2, 1920, 0);
    require(outside.status == arzoom::MultiPointerStatus::CursorOutside,
            "right-exclusive boundary guessed a screen");
}

void ambiguity_outside_and_utility_screens_never_guess()
{
    auto first = make_screen({0, 0, 1920, 1080}, 1920.0, 1080.0, {},
                             {0.0, 0.0, 960.0, 540.0});
    auto overlap = make_screen({0, 0, 1920, 1080}, 1920.0, 1080.0, {},
                               {960.0, 0.0, 1920.0, 540.0});
    const arzoom::MultiPresentationScreen ambiguous[] = {first, overlap};
    const auto ambiguous_result = arzoom::multi_resolve_scene_pointer(
        ambiguous, 2, 500, 500);
    require(ambiguous_result.status ==
                arzoom::MultiPointerStatus::AmbiguousPhysicalOwner,
            "overlapping physical ownership was guessed instead of rejected");

    overlap.eligible = false;
    const arzoom::MultiPresentationScreen with_utility[] = {first, overlap};
    const auto selected = arzoom::multi_resolve_scene_pointer(
        with_utility, 2, 500, 500);
    require(selected.ready() && selected.active_screen == 0,
            "unchecked utility screen participated in ownership");

    const auto outside = arzoom::multi_resolve_scene_pointer(
        with_utility, 2, 2500, 500);
    require(outside.status == arzoom::MultiPointerStatus::CursorOutside,
            "cursor outside eligible screens selected nearest/utility source");

    first.eligible = false;
    const arzoom::MultiPresentationScreen none[] = {first, overlap};
    const auto no_eligible = arzoom::multi_resolve_scene_pointer(
        none, 2, 500, 500);
    require(no_eligible.status ==
                arzoom::MultiPointerStatus::NoEligibleScreens,
            "zero eligible screens did not fail safe");
}

void crop_maps_visible_source_domain_and_rejects_hidden_pixels()
{
    const arzoom::MultiCrop crop{100.0, 50.0, 200.0, 150.0};
    const auto built = arzoom::multi_build_axis_aligned_transform(
        1000.0, 800.0, crop, {100.0, 200.0, 800.0, 800.0});
    require(built.ready(), "valid cropped axis-aligned transform rejected");

    const arzoom::MultiPresentationScreen screen = make_screen(
        {0, 0, 1000, 800}, 1000.0, 800.0, crop,
        {100.0, 200.0, 800.0, 800.0});

    const auto center = arzoom::multi_resolve_scene_pointer(
        &screen, 1, 500, 400);
    require(center.ready(), "visible cropped center did not resolve");
    require_near(center.scene_x, 500.0, 1.0e-9,
                 "cropped X did not preserve full-source coordinate truth");
    require_near(center.scene_y, 550.0, 1.0e-9,
                 "cropped Y did not preserve full-source coordinate truth");

    const auto hidden_left = arzoom::multi_resolve_scene_pointer(
        &screen, 1, 50, 400);
    require(hidden_left.status ==
                arzoom::MultiPointerStatus::CursorOutsideVisibleSource,
            "cursor in cropped-away source region was approximated into scene");

    const auto invalid_crop = arzoom::multi_build_axis_aligned_transform(
        1000.0, 800.0, {600.0, 0.0, 400.0, 0.0},
        {0.0, 0.0, 1000.0, 800.0});
    require(invalid_crop.status == arzoom::MultiGeometryStatus::InvalidCrop,
            "exhaustive crop was accepted");
}

void unsupported_transforms_and_invalid_geometry_fail_safe()
{
    arzoom::MultiPresentationScreen rotated = make_screen(
        {0, 0, 1920, 1080}, 1920.0, 1080.0, {},
        {0.0, 0.0, 1920.0, 1080.0});
    rotated.source_to_scene.xy = 0.25;
    const auto rotated_result = arzoom::multi_resolve_scene_pointer(
        &rotated, 1, 960, 540);
    require(rotated_result.status ==
                arzoom::MultiPointerStatus::UnsupportedGeometry &&
            rotated_result.geometry_status ==
                arzoom::MultiGeometryStatus::UnsupportedTransform,
            "rotation/skew-like transform was not rejected");

    auto flipped = rotated;
    flipped.source_to_scene.xy = 0.0;
    flipped.source_to_scene.xx = -1.0;
    const auto flipped_result = arzoom::multi_resolve_scene_pointer(
        &flipped, 1, 960, 540);
    require(flipped_result.status ==
                arzoom::MultiPointerStatus::UnsupportedGeometry &&
            flipped_result.geometry_status ==
                arzoom::MultiGeometryStatus::UnsupportedTransform,
            "flipped transform was not rejected");

    auto invalid_source = make_screen(
        {0, 0, 1920, 1080}, 1920.0, 1080.0, {},
        {0.0, 0.0, 1920.0, 1080.0});
    invalid_source.source_width = 0.0;
    const auto invalid_source_result = arzoom::multi_resolve_scene_pointer(
        &invalid_source, 1, 100, 100);
    require(invalid_source_result.status ==
                arzoom::MultiPointerStatus::UnsupportedGeometry &&
            invalid_source_result.geometry_status ==
                arzoom::MultiGeometryStatus::InvalidSourceSize,
            "zero source width did not fail safe");

    auto invalid_monitor = invalid_source;
    invalid_monitor.source_width = 1920.0;
    invalid_monitor.physical = {0, 0, 0, 1080};
    const auto invalid_monitor_result = arzoom::multi_resolve_scene_pointer(
        &invalid_monitor, 1, 100, 100);
    require(invalid_monitor_result.status ==
                arzoom::MultiPointerStatus::UnsupportedGeometry &&
            invalid_monitor_result.geometry_status ==
                arzoom::MultiGeometryStatus::InvalidPhysicalRect,
            "invalid physical rect was treated as cursor-outside guess");
}

void bounded_screen_cap_is_enforced()
{
    arzoom::MultiPresentationScreen
        screens[arzoom::kMultiMaxPresentationScreens + 1]{};
    const auto result = arzoom::multi_resolve_scene_pointer(
        screens, arzoom::kMultiMaxPresentationScreens + 1, 0, 0);
    require(result.status == arzoom::MultiPointerStatus::TooManyScreens,
            "screen cap overflow was not rejected before scanning");
}

void extreme_integer_desktop_coordinates_do_not_overflow_normalization()
{
    const std::int64_t low =
        std::numeric_limits<std::int64_t>::min() + 4096;
    const std::int64_t high =
        std::numeric_limits<std::int64_t>::max() - 4096;
    const arzoom::MultiPhysicalRect rect{low, -100, high, 100};
    require(rect.valid(), "extreme physical rect fixture invalid");
    const auto uv = arzoom::multi_physical_to_source_uv(rect, 0, 0);
    require(uv.x > 0.49 && uv.x < 0.51,
            "wide signed desktop normalization overflowed");
    require_near(uv.y, 0.5, 1.0e-12,
                 "extreme desktop Y normalization incorrect");
}

} // namespace

int main()
{
    two_independently_scaled_side_by_side_sources_map_correctly();
    negative_coordinates_and_half_open_boundaries_are_deterministic();
    ambiguity_outside_and_utility_screens_never_guess();
    crop_maps_visible_source_domain_and_rejects_hidden_pixels();
    unsupported_transforms_and_invalid_geometry_fail_safe();
    bounded_screen_cap_is_enforced();
    extreme_integer_desktop_coordinates_do_not_overflow_normalization();
    std::cout << "ArZoom M1 canonical coordinate engine gates: PASS\n";
    return 0;
}
