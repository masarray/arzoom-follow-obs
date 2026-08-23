#include "../src/arzoom-camera.hpp"

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

float edge_violation(const arzoom::CameraOutput &output)
{
    const float half = 0.5f / std::max(output.zoom, 1.0f);
    float violation = 0.0f;
    violation = std::max(violation, -(output.center.x - half));
    violation = std::max(violation, -(output.center.y - half));
    violation = std::max(violation, output.center.x + half - 1.0f);
    violation = std::max(violation, output.center.y + half - 1.0f);
    return std::max(0.0f, violation);
}

arzoom::CameraInput sample(float dt, arzoom::Vec2 cursor,
                           bool zoom_requested = true,
                           float zoom = 2.0f)
{
    arzoom::CameraInput input;
    input.dt = dt;
    input.cursor = cursor;
    input.cursor_valid = true;
    input.zoom_requested = zoom_requested;
    input.configured_zoom = zoom;
    input.follow_policy = arzoom::CameraFollowPolicy::Smart;
    input.motion_style = arzoom::CameraMotionStyle::Balanced;
    return input;
}

void warm_center(arzoom::SmartCamera &camera, int fps = 60,
                 float zoom = 2.0f)
{
    camera.step(sample(1.0f / fps, {0.5f, 0.5f}, false, zoom));
    for (int i = 0; i < fps; ++i)
        camera.step(sample(1.0f / fps, {0.5f, 0.5f}, true, zoom));
}

void activation_focus_continuity()
{
    using namespace arzoom;

    const Vec2 focuses[] = {
        {0.50f, 0.50f}, {0.05f, 0.50f}, {0.95f, 0.50f},
        {0.50f, 0.05f}, {0.50f, 0.95f}, {0.05f, 0.05f},
        {0.95f, 0.05f}, {0.05f, 0.95f}, {0.95f, 0.95f},
    };
    const float zooms[] = {2.0f, 3.0f, 4.0f};

    for (const Vec2 focus : focuses) {
        for (float zoom : zooms) {
            SmartCamera camera;
            camera.step(sample(1.0f / 60.0f, focus, false, zoom));

            const Vec2 expected = smart_follow_target(
                focus, {0.5f, 0.5f}, {0.5f, 0.45f}, 0.28f, zoom);
            const Vec2 direction = normalized(
                sub(expected, Vec2{0.5f, 0.5f}));
            float previous_projection = -1.0e-5f;

            for (int frame = 0; frame < 90; ++frame) {
                const CameraOutput output = camera.step(
                    sample(1.0f / 60.0f, focus, true, zoom));
                require(edge_violation(output) <= 2.0e-6f,
                        "activation exposed invalid source pixels");

                const Vec2 focus_output = cursor_output_position(
                    focus, output.center, output.zoom);
                require(focus_output.x >= -1.0e-4f &&
                            focus_output.x <= 1.0001f &&
                            focus_output.y >= -1.0e-4f &&
                            focus_output.y <= 1.0001f,
                        "latched activation focus left visible output");

                if (length(direction) > 0.0f &&
                    output.state == CameraState::Activating) {
                    const float projection = dot(
                        sub(output.center, Vec2{0.5f, 0.5f}), direction);
                    require(projection + 2.0e-5f >= previous_projection,
                            "activation detoured away from requested focus");
                    previous_projection = projection;
                }
            }

            require(std::fabs(camera.output().zoom - zoom) < 0.01f,
                    "activation did not reach requested zoom");
            require(length(sub(camera.output().center, expected)) < 0.01f,
                    "activation did not settle on legal focus framing");
        }
    }
}

void viewer_comfort_stays_locked()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);
    const Vec2 initial = camera.output().center;
    float max_displacement = 0.0f;

    for (int i = 0; i < 120; ++i) {
        const float t = static_cast<float>(i);
        const Vec2 cursor{
            0.5f + 0.015f * std::sin(t * 0.80f),
            0.5f + 0.015f * std::cos(t * 1.10f),
        };
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, cursor, true, 2.0f));
        max_displacement = std::max(
            max_displacement, length(sub(output.center, initial)));
    }
    require(max_displacement < 1.0e-5f,
            "normal hand jitter moved the gimbal camera");

    camera.reset();
    warm_center(camera);
    const Vec2 circle_start = camera.output().center;
    max_displacement = 0.0f;
    for (int i = 0; i < 120; ++i) {
        const float phase = 6.28318530718f *
                            static_cast<float>(i) / 120.0f;
        const Vec2 cursor{
            0.5f + 0.05f * std::cos(phase),
            0.5f + 0.05f * std::sin(phase),
        };
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, cursor, true, 2.0f));
        max_displacement = std::max(
            max_displacement, length(sub(output.center, circle_start)));
    }
    require(max_displacement < 1.0e-5f,
            "small explanation circle created camera wander");
}

void gimbal_glide_has_slow_start_no_overshoot()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);

    float previous_x = camera.output().center.x;
    float first_motion_speed = -1.0f;
    float max_speed = 0.0f;
    bool moved = false;

    for (int frame = 0; frame < 210; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, {0.90f, 0.50f}, true, 2.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "gimbal follow exposed invalid source pixels");
        require(output.center.x + 1.0e-5f >= previous_x,
                "stationary destination produced backwards camera hunting");
        require(output.center.x <= 0.7502f,
                "gimbal camera overshot the legal destination");

        const float speed = length(output.velocity) * output.zoom;
        if (!moved && speed > 0.0005f) {
            moved = true;
            first_motion_speed = speed;
        }
        max_speed = std::max(max_speed, speed);
        previous_x = output.center.x;
    }

    require(moved, "intentional relocation never moved the camera");
    require(first_motion_speed >= 0.0f && first_motion_speed < 0.10f,
            "gimbal camera did not start softly");
    require(max_speed < 1.35f,
            "default gimbal follow is too fast for viewer comfort");
    require(camera.output().center.x > 0.744f,
            "gimbal camera did not reach useful right-side framing");
    require(length(camera.output().velocity) * camera.output().zoom < 0.004f,
            "gimbal camera did not ease to a steady finish");
}

void moving_destination_retargets_without_restart()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);

    for (int frame = 0; frame < 45; ++frame)
        camera.step(sample(1.0f / 60.0f, {0.90f, 0.50f}, true, 2.0f));

    const CameraOutput before = camera.output();
    const CameraOutput first_after = camera.step(
        sample(1.0f / 60.0f, {0.10f, 0.50f}, true, 2.0f));

    const float first_step = length(sub(first_after.center, before.center));
    require(first_step < 0.012f,
            "retarget produced a visible one-frame viewport jump");
    require(first_after.velocity.x > -0.10f,
            "retarget instantly reversed the camera instead of bending path");

    float max_step = first_step;
    Vec2 previous = first_after.center;
    for (int frame = 0; frame < 210; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, {0.10f, 0.50f}, true, 2.0f));
        max_step = std::max(max_step,
                            length(sub(output.center, previous)));
        require(edge_violation(output) <= 2.0e-6f,
                "continuous retarget exposed invalid source pixels");
        previous = output.center;
    }

    require(max_step < 0.018f,
            "continuous retarget contained a visible camera snap");
    require(camera.output().center.x < 0.256f,
            "retargeted gimbal camera did not reach opposite framing");
    require(length(camera.output().velocity) * camera.output().zoom < 0.004f,
            "retargeted gimbal camera did not settle steadily");
}

void zoom_out_is_one_monotonic_minimum_jerk_shot()
{
    using namespace arzoom;

    SmartCamera camera;
    const Vec2 focus{0.95f, 0.16f};
    camera.step(sample(1.0f / 60.0f, focus, false, 3.0f));
    for (int frame = 0; frame < 180; ++frame)
        camera.step(sample(1.0f / 60.0f, focus, true, 3.0f));

    const CameraOutput start = camera.output();
    const Vec2 to_center = normalized(
        sub(Vec2{0.5f, 0.5f}, start.center));
    float previous_distance = length(sub(start.center, Vec2{0.5f, 0.5f}));
    float previous_zoom = start.zoom;
    Vec2 previous_center = start.center;
    float first_speed = -1.0f;

    for (int frame = 0; frame < 90; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, focus, false, 3.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "minimum-jerk zoom-out exposed invalid pixels");

        const float distance =
            length(sub(output.center, Vec2{0.5f, 0.5f}));
        require(distance <= previous_distance + 2.0e-6f,
                "zoom-out center reversed direction / wobbled");
        require(output.zoom <= previous_zoom + 2.0e-6f,
                "zoom-out magnification reversed direction");

        const Vec2 step = sub(output.center, previous_center);
        if (length(step) > 1.0e-7f) {
            require(dot(step, to_center) >= -2.0e-7f,
                    "zoom-out path hunted sideways near the finish");
        }
        if (first_speed < 0.0f)
            first_speed = length(output.velocity) * output.zoom;

        previous_distance = distance;
        previous_zoom = output.zoom;
        previous_center = output.center;
    }

    require(first_speed >= 0.0f && first_speed < 0.08f,
            "zoom-out did not begin with a soft minimum-jerk start");
    require(std::fabs(camera.output().zoom - 1.0f) < 1.0e-6f,
            "zoom-out did not finish exactly at 1x");
    require(length(sub(camera.output().center, Vec2{0.5f, 0.5f})) < 1.0e-6f,
            "zoom-out did not finish exactly at canvas center");
    require(camera.output().state == CameraState::Rest,
            "zoom-out did not enter exact steady lock");

    for (int frame = 0; frame < 120; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, focus, false, 3.0f));
        require(nearly_equal(output.center, {0.5f, 0.5f}, 1.0e-7f) &&
                    std::fabs(output.zoom - 1.0f) < 1.0e-7f,
                "camera breathed after zoom-out settle");
    }
}

void frame_rate_consistency()
{
    using namespace arzoom;

    const int fps_values[] = {30, 60, 120, 144};
    Vec2 centers[4];
    float zooms[4] = {};

    for (int index = 0; index < 4; ++index) {
        const int fps = fps_values[index];
        SmartCamera camera;
        camera.step(sample(1.0f / fps, {0.5f, 0.5f}, false, 2.0f));
        for (int frame = 0; frame < fps; ++frame)
            camera.step(sample(1.0f / fps, {0.5f, 0.5f}, true, 2.0f));
        for (int frame = 0; frame < 3 * fps; ++frame)
            camera.step(sample(1.0f / fps, {0.90f, 0.50f}, true, 2.0f));
        centers[index] = camera.output().center;
        zooms[index] = camera.output().zoom;
    }

    for (int index = 1; index < 4; ++index) {
        require(length(sub(centers[0], centers[index])) < 0.004f,
                "30/60/120/144 fps gimbal framing diverged");
        require(std::fabs(zooms[0] - zooms[index]) < 0.004f,
                "30/60/120/144 fps zoom diverged");
    }
}

} // namespace

int main()
{
    activation_focus_continuity();
    viewer_comfort_stays_locked();
    gimbal_glide_has_slow_start_no_overshoot();
    moving_destination_retargets_without_restart();
    zoom_out_is_one_monotonic_minimum_jerk_shot();
    frame_rate_consistency();
    std::cout << "ArZoom Smart Gimbal Camera 2.0 gates: PASS\n";
    return 0;
}
