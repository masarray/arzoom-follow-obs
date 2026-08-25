#include "../src/arzoom-scene-viewport-planner.hpp"

#include <algorithm>
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

arzoom::CameraInput make_input(float dt, arzoom::Vec2 cursor,
                               bool zoom_requested, float zoom)
{
    arzoom::CameraInput value;
    value.dt = dt;
    value.cursor = cursor;
    value.cursor_valid = true;
    value.zoom_requested = zoom_requested;
    value.configured_zoom = zoom;
    value.anchor = {0.5f, 0.45f};
    value.safe_zone = 0.28f;
    value.follow_policy = arzoom::CameraFollowPolicy::Smart;
    value.motion_style = arzoom::CameraMotionStyle::Balanced;
    return value;
}

void warm(arzoom::SceneViewportPlanner &camera,
          arzoom::Vec2 cursor, float zoom, float dt = 1.0f / 60.0f)
{
    camera.step(make_input(dt, cursor, false, zoom));
    for (int i = 0; i < 140; ++i)
        camera.step(make_input(dt, cursor, true, zoom));
}

void distance_guard_is_quiet_locally_but_arms_for_far_motion()
{
    using namespace arzoom;
    constexpr float zoom = 2.0f;
    const Vec2 center{0.5f, 0.5f};
    CameraInput value = make_input(1.0f / 60.0f, {0.5f, 0.5f}, true, zoom);

    value.cursor = {0.54f, 0.50f}; // ~58% output: local presentation work.
    require(!scene_adaptive_guard_needed(value, center, zoom, 0.010f),
            "normal-zoom local gesture incorrectly armed active tracking");

    value.cursor = {0.68f, 0.50f}; // ~86% output: far move at 2x.
    require(scene_adaptive_guard_needed(value, center, zoom, 0.010f),
            "normal-zoom far motion did not arm distance-adaptive tracking");

    CameraInput high = make_input(1.0f / 60.0f, {0.565f, 0.50f}, true, 4.0f);
    require(scene_adaptive_guard_needed(high, center, 4.0f, 0.010f),
            "4x smaller viewport did not arm tracking early enough");
}

void far_context_move_is_more_decisive_than_small_reframe()
{
    using namespace arzoom;
    const CameraProfile profile = camera_profile(CameraMotionStyle::Balanced);
    const float near_seconds = scene_context_shot_seconds(profile, 2.0f, 0.06f);
    const float far_seconds = scene_context_shot_seconds(profile, 2.0f, 0.50f);
    require(far_seconds < near_seconds * 0.80f,
            "far viewport move was not materially faster than a small reframe");

    const float far_high = scene_context_shot_seconds(profile, 4.0f, 0.50f);
    require(far_high < far_seconds,
            "high-zoom far reframe did not become more decisive");
}

struct SweepResult {
    int max_consecutive_invisible = 0;
    float max_step_output = 0.0f;
    arzoom::Vec2 final_pointer{0.5f, 0.5f};
    arzoom::Vec2 final_center{0.5f, 0.5f};
    unsigned long long final_generation = 0;
};

SweepResult run_far_sweep(float zoom, arzoom::Vec2 far_cursor,
                          arzoom::Vec2 final_cursor)
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    warm(camera, {0.50f, 0.50f}, zoom, dt);

    SweepResult result;
    int invisible = 0;
    Vec2 previous_center = camera.output().center;

    const auto sample = [&](Vec2 cursor) {
        const CameraOutput out = camera.step(make_input(dt, cursor, true, zoom));
        const Vec2 pointer = cursor_output_position(cursor, out.center, out.zoom);
        const bool visible = pointer.x >= 0.0f && pointer.x <= 1.0f &&
                             pointer.y >= 0.0f && pointer.y <= 1.0f;
        invisible = visible ? 0 : invisible + 1;
        result.max_consecutive_invisible = std::max(
            result.max_consecutive_invisible, invisible);
        result.max_step_output = std::max(
            result.max_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_center = out.center;
    };

    for (int frame = 1; frame <= 34; ++frame) {
        const float t = static_cast<float>(frame) / 34.0f;
        sample(lerp({0.50f, 0.50f}, far_cursor, t));
    }
    for (int frame = 1; frame <= 34; ++frame) {
        const float t = static_cast<float>(frame) / 34.0f;
        sample(lerp(far_cursor, final_cursor, t));
    }

    for (int frame = 0; frame < 120; ++frame)
        sample(final_cursor);

    result.final_center = camera.output().center;
    result.final_pointer = cursor_output_position(
        final_cursor, camera.output().center, camera.output().zoom);
    result.final_generation = camera.generation();

    const Vec2 hold_center = camera.output().center;
    const unsigned long long hold_generation = camera.generation();
    for (int frame = 0; frame < 180; ++frame) {
        const CameraOutput out = camera.step(
            make_input(dt, final_cursor, true, zoom));
        require(length(sub(out.center, hold_center)) < 1.0e-7f,
                "final HOLD drifted after far pointer acquisition");
        require(camera.generation() == hold_generation,
                "final HOLD generated a new target without pointer intent");
    }

    return result;
}

void normal_zoom_far_motion_stays_visible_and_calm()
{
    const SweepResult result = run_far_sweep(
        2.0f, {0.88f, 0.73f}, {0.69f, 0.62f});
    require(result.max_consecutive_invisible <= 5,
            "2x far-moving pointer stayed outside viewport too long");
    require(result.max_step_output < 0.090f,
            "2x distance-adaptive tracking introduced a viewport snap");
    require(result.final_pointer.x >= 0.36f && result.final_pointer.x <= 0.64f &&
                result.final_pointer.y >= 0.31f && result.final_pointer.y <= 0.59f,
            "2x final pointer did not land in optimal contextual area");
}

void high_zoom_far_motion_stays_visible_and_more_central()
{
    const SweepResult result = run_far_sweep(
        4.0f, {0.80f, 0.70f}, {0.61f, 0.57f});
    require(result.max_consecutive_invisible <= 4,
            "4x far-moving pointer stayed outside viewport too long");
    require(result.max_step_output < 0.100f,
            "4x adaptive tracking introduced a viewport snap");
    require(result.final_pointer.x >= 0.40f && result.final_pointer.x <= 0.60f &&
                result.final_pointer.y >= 0.35f && result.final_pointer.y <= 0.55f,
            "4x final pointer did not land near the optimal central context");
}

void predictive_guard_does_not_turn_local_motion_into_cursor_chasing()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    warm(camera, {0.50f, 0.50f}, 2.0f, dt);
    const Vec2 start_center = camera.output().center;

    for (int frame = 1; frame <= 30; ++frame) {
        const float t = static_cast<float>(frame) / 30.0f;
        camera.step(make_input(dt, lerp({0.50f, 0.50f}, {0.54f, 0.51f}, t),
                               true, 2.0f));
    }
    for (int frame = 1; frame <= 30; ++frame) {
        const float t = static_cast<float>(frame) / 30.0f;
        camera.step(make_input(dt, lerp({0.54f, 0.51f}, {0.50f, 0.50f}, t),
                               true, 2.0f));
    }
    for (int frame = 0; frame < 60; ++frame)
        camera.step(make_input(dt, {0.50f, 0.50f}, true, 2.0f));

    require(length(sub(camera.output().center, start_center)) < 1.0e-6f,
            "local default-zoom gesture caused unwanted camera chasing");
}

} // namespace

int main()
{
    distance_guard_is_quiet_locally_but_arms_for_far_motion();
    far_context_move_is_more_decisive_than_small_reframe();
    normal_zoom_far_motion_stays_visible_and_calm();
    high_zoom_far_motion_stays_visible_and_more_central();
    predictive_guard_does_not_turn_local_motion_into_cursor_chasing();
    std::cout << "ArZoom P4.1 distance-adaptive follow gates: PASS\n";
    return 0;
}
