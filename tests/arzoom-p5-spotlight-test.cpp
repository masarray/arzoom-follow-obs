#include "arzoom-spotlight.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool near(float a, float b, float eps = 1.0e-4f)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

int main()
{
    using namespace arzoom;

    const auto compact = spotlight_half_size_px(
        SpotlightSize::Compact, 1920.0f, 1080.0f);
    const auto balanced = spotlight_half_size_px(
        SpotlightSize::Balanced, 1920.0f, 1080.0f);
    const auto wide = spotlight_half_size_px(
        SpotlightSize::Wide, 1920.0f, 1080.0f);
    assert(compact.x < balanced.x && balanced.x < wide.x);
    assert(compact.y < balanced.y && balanced.y < wide.y);

    const SpotlightVec2 radius{300.0f, 180.0f};
    const float ellipse_center = spotlight_ellipse_signed_distance_px(
        {0.0f, 0.0f}, radius);
    const float ellipse_edge = spotlight_ellipse_signed_distance_px(
        {300.0f, 0.0f}, radius);
    const float ellipse_outside = spotlight_ellipse_signed_distance_px(
        {450.0f, 0.0f}, radius);
    assert(ellipse_center < 0.0f);
    assert(near(ellipse_edge, 0.0f));
    assert(ellipse_outside > 0.0f);

    const float rect_center = spotlight_rounded_rect_signed_distance_px(
        {0.0f, 0.0f}, {300.0f, 180.0f}, 40.0f);
    const float rect_outside = spotlight_rounded_rect_signed_distance_px(
        {420.0f, 260.0f}, {300.0f, 180.0f}, 40.0f);
    assert(rect_center < 0.0f);
    assert(rect_outside > 0.0f);

    assert(near(spotlight_scene_multiplier(200.0f, 90.0f, 0.38f, false),
                1.0f));
    assert(near(spotlight_scene_multiplier(-20.0f, 90.0f, 0.38f, true),
                1.0f));
    assert(near(spotlight_scene_multiplier(200.0f, 90.0f, 0.38f, true),
                0.62f));

    const float feather_near = spotlight_scene_multiplier(
        15.0f, 90.0f, 0.38f, true);
    const float feather_mid = spotlight_scene_multiplier(
        45.0f, 90.0f, 0.38f, true);
    const float feather_far = spotlight_scene_multiplier(
        75.0f, 90.0f, 0.38f, true);
    assert(feather_near > feather_mid);
    assert(feather_mid > feather_far);
    assert(feather_near < 1.0f && feather_far > 0.62f);

    const auto at_1080 = spotlight_half_size_px(
        SpotlightSize::Balanced, 1920.0f, 1080.0f);
    const auto at_4k = spotlight_half_size_px(
        SpotlightSize::Balanced, 3840.0f, 2160.0f);
    assert(near(at_4k.x / at_1080.x, 2.0f));
    assert(near(at_4k.y / at_1080.y, 2.0f));

    std::cout << "P5 Spotlight primitive invariants passed\n";
    return 0;
}
