#include "../src/arzoom-scene-motion-synthesizer.hpp"
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

void kinematic_target_has_bounded_jerk_and_no_stop_start()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    constexpr float zoom = 4.0f;
    SceneKinematicMotion motion;
    motion.reset({0.50f, 0.50f});
    const Vec2 target{0.68f, 0.58f};
    const float distance_output = length(sub(target, Vec2{0.50f, 0.50f})) * zoom;
    const SceneMotionLimits limits = scene_motion_limits(
        CameraMotionStyle::Balanced, zoom, 0.90f, distance_output);

    Vec2 previous_acceleration{0.0f, 0.0f};
    float max_jerk = 0.0f;
    int unexpected_stalls = 0;
    bool had_motion = false;
    SceneMotionSample sample;

    for (int frame = 0; frame < 240; ++frame) {
        sample = motion.step(target, zoom, dt, limits);
        const Vec2 acceleration_output = mul(sample.acceleration, zoom);
        const float jerk = length(sub(acceleration_output, previous_acceleration)) / dt;
        max_jerk = std::max(max_jerk, jerk);
        previous_acceleration = acceleration_output;

        const float remaining = length(sub(target, sample.center)) * zoom;
        const float speed = length(sample.velocity) * zoom;
        if (speed > 0.025f)
            had_motion = true;
        if (had_motion && remaining > 0.08f && speed < 0.004f)
            ++unexpected_stalls;
        if (sample.settled)
            break;
    }

    require(sample.settled, "kinematic camera failed to settle at a fixed target");
    require(max_jerk <= limits.max_jerk_output * 1.08f,
            "kinematic camera exceeded jerk contract");
    require(unexpected_stalls == 0,
            "kinematic camera stopped and restarted while target was still far");
}

void retarget_keeps_velocity_continuous()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    constexpr float zoom = 3.0f;
    SceneKinematicMotion motion;
    motion.reset({0.50f, 0.50f});
    const SceneMotionLimits limits = scene_motion_limits(
        CameraMotionStyle::Balanced, zoom, 0.75f, 0.55f);

    SceneMotionSample sample;
    for (int frame = 0; frame < 10; ++frame)
        sample = motion.step({0.64f, 0.50f}, zoom, dt, limits);

    const float speed_before = length(sample.velocity) * zoom;
    const Vec2 acceleration_before = mul(sample.acceleration, zoom);
    sample = motion.step({0.72f, 0.54f}, zoom, dt, limits);
    const float speed_after = length(sample.velocity) * zoom;
    const float handoff_jerk =
        length(sub(mul(sample.acceleration, zoom), acceleration_before)) / dt;

    require(speed_before > 0.08f,
            "retarget continuity trace did not establish camera motion");
    require(speed_after > speed_before * 0.55f,
            "retarget hard-reset camera velocity");
    require(handoff_jerk <= limits.max_jerk_output * 1.08f,
            "retarget introduced an acceleration discontinuity");
}

void direction_reversal_is_single_and_jerk_limited()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    constexpr float zoom = 3.0f;
    SceneKinematicMotion motion;
    motion.reset({0.50f, 0.50f});
    const SceneMotionLimits limits = scene_motion_limits(
        CameraMotionStyle::Balanced, zoom, 0.85f, 0.60f);

    SceneMotionSample sample;
    for (int frame = 0; frame < 18; ++frame)
        sample = motion.step({0.69f, 0.50f}, zoom, dt, limits);

    Vec2 previous_acceleration = mul(sample.acceleration, zoom);
    int sign_changes = 0;
    int previous_sign = sample.velocity.x > 0.002f ? 1 :
                        (sample.velocity.x < -0.002f ? -1 : 0);
    float max_jerk = 0.0f;

    for (int frame = 0; frame < 180; ++frame) {
        sample = motion.step({0.36f, 0.50f}, zoom, dt, limits);
        const int sign = sample.velocity.x > 0.002f ? 1 :
                         (sample.velocity.x < -0.002f ? -1 : 0);
        if (sign != 0 && previous_sign != 0 && sign != previous_sign)
            ++sign_changes;
        if (sign != 0)
            previous_sign = sign;

        const Vec2 acceleration = mul(sample.acceleration, zoom);
        max_jerk = std::max(
            max_jerk, length(sub(acceleration, previous_acceleration)) / dt);
        previous_acceleration = acceleration;
        if (sample.settled)
            break;
    }

    require(sample.settled, "reversal trace failed to settle");
    require(sign_changes <= 1,
            "camera oscillated through repeated velocity direction reversals");
    require(max_jerk <= limits.max_jerk_output * 1.08f,
            "direction reversal exceeded jerk contract");
}

void follow_pressure_is_continuous()
{
    using namespace arzoom;
    CameraInput value = make_input(1.0f / 60.0f, {0.5f, 0.5f}, true, 3.0f);
    const Vec2 center{0.5f, 0.5f};
    float previous = 0.0f;
    float maximum_jump = 0.0f;

    for (int step = 0; step <= 100; ++step) {
        const float output_x = 0.50f + 0.005f * static_cast<float>(step);
        value.cursor.x = center.x + (output_x - 0.5f) / 3.0f;
        value.cursor.y = center.y + (0.45f - 0.5f) / 3.0f;
        const float pressure = scene_follow_pressure(value, center, 3.0f);
        if (step > 0)
            maximum_jump = std::max(maximum_jump, std::fabs(pressure - previous));
        require(pressure + 1.0e-5f >= previous,
                "follow pressure moved backward while pointer approached edge");
        previous = pressure;
    }

    require(maximum_jump < 0.10f,
            "follow pressure contains a gear-shift discontinuity");
}

struct PlannerMotionMetrics {
    int tracking_entries = 0;
    int active_zero_speed_stalls = 0;
    int velocity_sign_reversals_x = 0;
    float max_jerk_output = 0.0f;
    int max_invisible_frames = 0;
};

PlannerMotionMetrics run_planner_sweep(float zoom)
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    SceneViewportPlanner planner;
    Vec2 cursor{0.50f, 0.50f};
    planner.step(make_input(dt, cursor, false, zoom));
    for (int frame = 0; frame < 140; ++frame)
        planner.step(make_input(dt, cursor, true, zoom));

    PlannerMotionMetrics metrics;
    Vec2 previous_acceleration = planner.output().acceleration;
    int previous_sign = 0;
    int invisible = 0;

    const auto sample = [&](Vec2 pointer) {
        const CameraOutput out = planner.step(make_input(dt, pointer, true, zoom));
        const Vec2 pointer_output = cursor_output_position(pointer, out.center, out.zoom);
        const bool visible = pointer_output.x >= 0.0f && pointer_output.x <= 1.0f &&
                             pointer_output.y >= 0.0f && pointer_output.y <= 1.0f;
        invisible = visible ? 0 : invisible + 1;
        metrics.max_invisible_frames = std::max(metrics.max_invisible_frames,
                                                invisible);

        const float speed_output = length(out.velocity) * out.zoom;
        const float pressure = scene_follow_pressure(
            make_input(dt, pointer, true, zoom), out.center, out.zoom);
        if (planner.guard_follow_active() && pressure > 0.35f &&
            speed_output < 0.003f)
            ++metrics.active_zero_speed_stalls;

        const int sign = out.velocity.x > 0.0015f ? 1 :
                         (out.velocity.x < -0.0015f ? -1 : 0);
        if (sign != 0 && previous_sign != 0 && sign != previous_sign)
            ++metrics.velocity_sign_reversals_x;
        if (sign != 0)
            previous_sign = sign;

        const Vec2 acceleration_output = mul(out.acceleration, out.zoom);
        const Vec2 previous_output = mul(previous_acceleration, out.zoom);
        metrics.max_jerk_output = std::max(
            metrics.max_jerk_output,
            length(sub(acceleration_output, previous_output)) / dt);
        previous_acceleration = out.acceleration;
    };

    for (int frame = 1; frame <= 42; ++frame) {
        const float t = static_cast<float>(frame) / 42.0f;
        sample(lerp({0.50f, 0.50f}, {0.88f, 0.64f}, t));
    }
    for (int frame = 0; frame < 28; ++frame)
        sample({0.88f, 0.64f});

    metrics.tracking_entries = static_cast<int>(planner.tracking_entries());

    for (int frame = 0; frame < 180; ++frame)
        sample({0.88f, 0.64f});

    const CameraOutput held = planner.output();
    const Vec2 hold_center = held.center;
    const unsigned long long hold_generation = planner.generation();
    for (int frame = 0; frame < 180; ++frame) {
        const CameraOutput out = planner.step(
            make_input(dt, {0.88f, 0.64f}, true, zoom));
        require(length(sub(out.center, hold_center)) < 1.0e-7f,
                "planner drifted after kinematic settle");
        require(planner.generation() == hold_generation,
                "planner regenerated target during exact HOLD");
    }

    return metrics;
}

void planner_far_follow_is_latched_and_smooth()
{
    const PlannerMotionMetrics normal = run_planner_sweep(2.0f);
    require(normal.tracking_entries <= 2,
            "normal-zoom far gesture chattered tracking ON/OFF");
    require(normal.active_zero_speed_stalls == 0,
            "normal-zoom tracking contains stop-start stalls");
    require(normal.velocity_sign_reversals_x <= 1,
            "normal-zoom camera looked indecisive through repeated reversals");
    require(normal.max_invisible_frames <= 6,
            "normal-zoom camera lost pointer for too long");

    const PlannerMotionMetrics high = run_planner_sweep(4.0f);
    require(high.tracking_entries <= 2,
            "high-zoom far gesture chattered tracking ON/OFF");
    require(high.active_zero_speed_stalls == 0,
            "high-zoom tracking contains stop-start stalls");
    require(high.velocity_sign_reversals_x <= 1,
            "high-zoom camera looked indecisive through repeated reversals");
    require(high.max_invisible_frames <= 5,
            "high-zoom camera lost pointer for too long");
}

} // namespace

int main()
{
    kinematic_target_has_bounded_jerk_and_no_stop_start();
    retarget_keeps_velocity_continuous();
    direction_reversal_is_single_and_jerk_limited();
    follow_pressure_is_continuous();
    planner_far_follow_is_latched_and_smooth();
    std::cout << "ArZoom P4.1 motion quality gates: PASS\n";
    return 0;
}
