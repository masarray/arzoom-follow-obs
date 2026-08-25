#include "../src/arzoom-camera.hpp"
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

arzoom::CameraInput input(float dt, arzoom::Vec2 cursor,
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
    camera.step(input(dt, cursor, false, zoom));
    for (int i = 0; i < 120; ++i)
        camera.step(input(dt, cursor, true, zoom));
}

void repeated_zoom_out_never_freezes()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    const Vec2 cursor{0.84f, 0.72f};
    warm(camera, cursor, 3.0f);

    float target = 3.0f;
    float previous_zoom = camera.output().zoom;
    float max_frame_delta = 0.0f;

    for (int step = 0; step < 4; ++step) {
        target -= 0.25f;
        bool moved = false;
        for (int frame = 0; frame < 80; ++frame) {
            const CameraOutput out = camera.step(input(dt, cursor, true, target));
            max_frame_delta = std::max(
                max_frame_delta, std::fabs(out.zoom - previous_zoom));
            if (out.zoom < previous_zoom - 1.0e-5f)
                moved = true;
            require(out.zoom <= previous_zoom + 2.0e-4f,
                    "active Zoom Out reversed direction");
            previous_zoom = out.zoom;
        }
        require(moved, "active Zoom Out froze instead of moving");
        require(std::fabs(camera.output().zoom - target) < 0.002f,
                "active Zoom Out did not settle exactly at requested level");
    }

    require(max_frame_delta < 0.050f,
            "active Zoom Out contained a visible one-frame snap");
}

void zoom_step_is_pointer_anchored_and_exact()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    const Vec2 cursor{0.73f, 0.62f};
    warm(camera, cursor, 2.0f);

    const float target_zoom = 2.75f;
    Vec2 previous_center = camera.output().center;
    float previous_zoom = camera.output().zoom;
    float max_center_step_output = 0.0f;
    float max_zoom_step = 0.0f;

    for (int frame = 0; frame < 100; ++frame) {
        const CameraOutput out = camera.step(
            input(dt, cursor, true, target_zoom));
        max_center_step_output = std::max(
            max_center_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        max_zoom_step = std::max(max_zoom_step,
                                 std::fabs(out.zoom - previous_zoom));
        previous_center = out.center;
        previous_zoom = out.zoom;
    }

    const CameraOutput settled = camera.output();
    const Vec2 pointer = cursor_output_position(cursor, settled.center,
                                                settled.zoom);
    require(std::fabs(settled.zoom - target_zoom) < 0.002f,
            "Zoom step did not exact-settle at requested level");
    require(std::fabs(pointer.x - 0.5f) < 0.025f &&
                std::fabs(pointer.y - 0.45f) < 0.025f,
            "Zoom step did not use final pointer as semantic anchor");
    require(max_center_step_output < 0.055f,
            "pointer-anchored Zoom produced a pan snap");
    require(max_zoom_step < 0.065f,
            "pointer-anchored Zoom produced a scale snap");
}

void rule_of_thirds_plan_is_symmetric()
{
    using namespace arzoom;
    CameraInput value = input(1.0f / 60.0f, {0.5f, 0.5f}, true, 3.5f);
    const Vec2 center{0.5f, 0.5f};
    constexpr float zoom = 3.5f;

    const auto cursor_for_output = [&](Vec2 output) {
        return Vec2{
            center.x + (output.x - 0.5f) / zoom,
            center.y + (output.y - 0.5f) / zoom,
        };
    };

    value.cursor = cursor_for_output({0.34f, 0.45f});
    const auto left = scene_context_plan(value, center, zoom);
    require(left.active && left.target_center.x < center.x,
            "left rule-of-thirds final pointer did not request one reframe");

    value.cursor = cursor_for_output({0.66f, 0.45f});
    const auto right = scene_context_plan(value, center, zoom);
    require(right.active && right.target_center.x > center.x,
            "right rule-of-thirds final pointer did not request one reframe");

    value.cursor = cursor_for_output({0.50f, 0.30f});
    const auto top = scene_context_plan(value, center, zoom);
    require(top.active && top.target_center.y < center.y,
            "top rule-of-thirds final pointer did not request one reframe");

    value.cursor = cursor_for_output({0.50f, 0.62f});
    const auto bottom = scene_context_plan(value, center, zoom);
    require(bottom.active && bottom.target_center.y > center.y,
            "bottom rule-of-thirds final pointer did not request one reframe");

    value.cursor = cursor_for_output({0.50f, 0.45f});
    require(!scene_context_plan(value, center, zoom).active,
            "well-framed pointer incorrectly requested camera movement");
}

void high_zoom_profile_becomes_more_attentive_without_changing_default()
{
    using namespace arzoom;

    require(std::fabs(scene_pointer_settle_seconds(
                          CameraMotionStyle::Balanced, 2.0f) - 0.12f) < 1.0e-6f,
            "default zoom changed the proven Balanced settle timing");
    require(scene_zoom_pressure(2.0f) == 0.0f,
            "default zoom unexpectedly enabled high-zoom pressure");

    const float settle_default = scene_pointer_settle_seconds(
        CameraMotionStyle::Balanced, 2.0f);
    const float settle_high = scene_pointer_settle_seconds(
        CameraMotionStyle::Balanced, 4.0f);
    require(settle_high < settle_default * 0.65f,
            "high zoom did not shorten final-pointer decision time enough");

    const float wake_default = scene_context_wake_half(0.28f, 2.0f);
    const float wake_high = scene_context_wake_half(0.28f, 4.0f);
    require(wake_high < wake_default * 0.80f,
            "high zoom did not tighten contextual pointer envelope");

    const float landing_default = scene_context_landing_half(
        wake_default, 2.0f);
    const float landing_high = scene_context_landing_half(
        wake_high, 4.0f);
    require(landing_high < landing_default * 0.65f,
            "high zoom did not land pointer closer to the presentation anchor");

    CameraInput value = input(1.0f / 60.0f, {0.5f, 0.5f}, true, 4.0f);
    const Vec2 center{0.5f, 0.5f};
    value.cursor = {
        center.x + (0.90f - 0.5f) / 4.0f,
        center.y,
    };
    require(scene_high_zoom_guard_needed(value, center, 4.0f),
            "high zoom did not arm edge safety guard");
    require(!scene_high_zoom_guard_needed(value, center, 2.0f),
            "default zoom incorrectly armed high-zoom edge guard");
}

void high_zoom_right_to_center_recovers_final_pointer()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    Vec2 cursor{0.50f, 0.50f};
    float zoom = 2.0f;
    warm(camera, cursor, zoom);

    for (int step = 0; step < 6; ++step) {
        zoom = std::min(4.0f, zoom + 0.25f);
        for (int frame = 0; frame < 60; ++frame)
            camera.step(input(dt, cursor, true, zoom));
    }

    cursor = {0.91f, 0.52f};
    for (int frame = 0; frame < 150; ++frame)
        camera.step(input(dt, cursor, true, zoom));

    const Vec2 final_cursor{0.50f, 0.50f};
    for (int frame = 0; frame < 180; ++frame)
        camera.step(input(dt, final_cursor, true, zoom));

    const CameraOutput settled = camera.output();
    const Vec2 pointer = cursor_output_position(
        final_cursor, settled.center, settled.zoom);
    require(pointer.x >= 0.40f && pointer.x <= 0.60f &&
                pointer.y >= 0.35f && pointer.y <= 0.55f,
            "high zoom right-to-centre move did not keep final pointer near optimal area");
}

void high_zoom_guard_prevents_long_pointer_loss_during_motion()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    constexpr float zoom = 4.0f;
    SceneViewportPlanner camera;
    warm(camera, {0.50f, 0.50f}, zoom);

    int consecutive_invisible = 0;
    int max_consecutive_invisible = 0;
    float max_step_output = 0.0f;
    Vec2 previous_center = camera.output().center;

    const auto sample = [&](Vec2 cursor, SceneViewportPlanner &planner,
                            int &invisible, int &max_invisible,
                            float &max_step, Vec2 &previous) {
        const CameraOutput out = planner.step(input(dt, cursor, true, zoom));
        const Vec2 pointer = cursor_output_position(cursor, out.center, out.zoom);
        const bool visible = pointer.x >= 0.0f && pointer.x <= 1.0f &&
                             pointer.y >= 0.0f && pointer.y <= 1.0f;
        if (visible)
            invisible = 0;
        else
            ++invisible;
        max_invisible = std::max(max_invisible, invisible);
        max_step = std::max(max_step,
                            length(sub(out.center, previous)) * out.zoom);
        previous = out.center;
    };

    for (int frame = 1; frame <= 36; ++frame) {
        const float t = static_cast<float>(frame) / 36.0f;
        const Vec2 cursor = lerp({0.50f, 0.50f}, {0.90f, 0.54f}, t);
        sample(cursor, camera, consecutive_invisible,
               max_consecutive_invisible, max_step_output, previous_center);
    }
    for (int frame = 1; frame <= 36; ++frame) {
        const float t = static_cast<float>(frame) / 36.0f;
        const Vec2 cursor = lerp({0.90f, 0.54f}, {0.52f, 0.50f}, t);
        sample(cursor, camera, consecutive_invisible,
               max_consecutive_invisible, max_step_output, previous_center);
    }

    const Vec2 final_cursor{0.52f, 0.50f};
    for (int frame = 0; frame < 90; ++frame)
        camera.step(input(dt, final_cursor, true, zoom));

    const Vec2 final_pointer = cursor_output_position(
        final_cursor, camera.output().center, camera.output().zoom);
    require(max_consecutive_invisible <= 6,
            "4x moving pointer remained outside viewport too long before guard recovery");
    require(final_pointer.x >= 0.40f && final_pointer.x <= 0.60f &&
                final_pointer.y >= 0.35f && final_pointer.y <= 0.55f,
            "4x guard failed to finish with pointer in optimal context area");
    require(max_step_output < 0.080f,
            "high-zoom guard introduced a viewport snap");
}

void one_plan_one_shot_then_exact_hold()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner camera;
    warm(camera, {0.50f, 0.50f}, 3.5f);

    Vec2 cursor{0.50f, 0.50f};
    const Vec2 final_cursor{0.72f, 0.64f};
    for (int frame = 1; frame <= 45; ++frame) {
        const float t = static_cast<float>(frame) / 45.0f;
        cursor = lerp({0.50f, 0.50f}, final_cursor, t);
        camera.step(input(dt, cursor, true, 3.5f));
    }

    const unsigned long long before_settle_generation = camera.generation();
    for (int frame = 0; frame < 90; ++frame)
        camera.step(input(dt, final_cursor, true, 3.5f));
    const unsigned long long settled_generation = camera.generation();

    require(settled_generation <= before_settle_generation + 2,
            "settled high-zoom pointer kept creating competing viewport plans");

    const Vec2 hold_center = camera.output().center;
    const float hold_zoom = camera.output().zoom;
    const unsigned long long hold_generation = camera.generation();
    float max_hold_drift = 0.0f;

    for (int frame = 0; frame < 240; ++frame) {
        const CameraOutput out = camera.step(
            input(dt, final_cursor, true, 3.5f));
        max_hold_drift = std::max(
            max_hold_drift, length(sub(out.center, hold_center)));
        require(std::fabs(out.zoom - hold_zoom) < 1.0e-7f,
                "exact HOLD allowed zoom drift");
        require(camera.generation() == hold_generation,
                "exact HOLD generated a second target without new intent");
    }

    require(max_hold_drift < 1.0e-7f,
            "viewport oscillated after final target was settled");
    require(length(camera.output().velocity) < 1.0e-7f &&
                length(camera.output().acceleration) < 1.0e-7f,
            "HOLD retained hidden motion after exact settle");
}

arzoom::Vec2 run_fps_trace(float fps)
{
    using namespace arzoom;
    const float dt = 1.0f / fps;
    SceneViewportPlanner camera;
    warm(camera, {0.50f, 0.50f}, 3.25f, dt);

    const Vec2 final_cursor{0.31f, 0.73f};
    const int move_frames = static_cast<int>(0.55f * fps);
    for (int frame = 1; frame <= move_frames; ++frame) {
        const float t = static_cast<float>(frame) /
                        static_cast<float>(move_frames);
        const Vec2 cursor = lerp({0.50f, 0.50f}, final_cursor, t);
        camera.step(input(dt, cursor, true, 3.25f));
    }
    const int hold_frames = static_cast<int>(2.0f * fps);
    for (int frame = 0; frame < hold_frames; ++frame)
        camera.step(input(dt, final_cursor, true, 3.25f));
    return camera.output().center;
}

void final_framing_is_frame_rate_independent()
{
    using namespace arzoom;
    const Vec2 c30 = run_fps_trace(30.0f);
    const Vec2 c60 = run_fps_trace(60.0f);
    const Vec2 c144 = run_fps_trace(144.0f);
    require(length(sub(c30, c60)) < 0.003f,
            "30fps and 60fps produced different final viewport framing");
    require(length(sub(c60, c144)) < 0.003f,
            "60fps and 144fps produced different final viewport framing");
}

void per_source_facade_still_delegates_to_legacy_camera()
{
    using namespace arzoom;
    PresenterAwareSmartCamera facade;
    CameraInput value = input(1.0f / 60.0f, {0.75f, 0.65f}, true, 2.0f);
    /* set_scene_context(false) is the default: this is the legacy P1 path. */
    for (int frame = 0; frame < 120; ++frame)
        facade.step(value);
    require(facade.output().zoom > 1.5f,
            "per-source facade stopped delegating to legacy SmartCamera");
}

} // namespace

int main()
{
    repeated_zoom_out_never_freezes();
    zoom_step_is_pointer_anchored_and_exact();
    rule_of_thirds_plan_is_symmetric();
    high_zoom_profile_becomes_more_attentive_without_changing_default();
    high_zoom_right_to_center_recovers_final_pointer();
    high_zoom_guard_prevents_long_pointer_loss_during_motion();
    one_plan_one_shot_then_exact_hold();
    final_framing_is_frame_rate_independent();
    per_source_facade_still_delegates_to_legacy_camera();
    std::cout << "ArZoom P4.1 zoom-adaptive deterministic viewport gates: PASS\n";
    return 0;
}
