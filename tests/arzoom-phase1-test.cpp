#include "../src/arzoom-camera.hpp"

#include <cassert>
#include <cmath>
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

arzoom::CameraInput input(float dt, arzoom::Vec2 cursor,
                          bool zoom_requested = true,
                          float zoom = 2.0f)
{
    arzoom::CameraInput result;
    result.dt = dt;
    result.cursor = cursor;
    result.cursor_valid = true;
    result.zoom_requested = zoom_requested;
    result.configured_zoom = zoom;
    result.follow_policy = arzoom::CameraFollowPolicy::Smart;
    result.motion_style = arzoom::CameraMotionStyle::Balanced;
    return result;
}

void warm_center(arzoom::SmartCamera &camera, int fps = 60,
                 float zoom = 2.0f)
{
    camera.step(input(1.0f / fps, {0.5f, 0.5f}, false, zoom));
    for (int i = 0; i < fps; ++i)
        camera.step(input(1.0f / fps, {0.5f, 0.5f}, true, zoom));
}

void test_activation_focus_continuity()
{
    using namespace arzoom;

    const Vec2 focuses[] = {
        {0.50f, 0.50f},
        {0.05f, 0.50f}, {0.95f, 0.50f},
        {0.50f, 0.05f}, {0.50f, 0.95f},
        {0.05f, 0.05f}, {0.95f, 0.05f},
        {0.05f, 0.95f}, {0.95f, 0.95f},
    };
    const float zooms[] = {2.0f, 3.0f, 4.0f};

    for (const Vec2 focus : focuses) {
        for (const float zoom : zooms) {
            SmartCamera camera;
            camera.step(input(1.0f / 60.0f, focus, false, zoom));

            const Vec2 final_center = smart_follow_target(
                focus, {0.5f, 0.5f}, {0.5f, 0.45f}, 0.28f, zoom);
            const Vec2 direction = normalized(
                sub(final_center, Vec2{0.5f, 0.5f}));
            float previous_projection = -1.0e-5f;

            for (int frame = 0; frame < 90; ++frame) {
                const CameraOutput output = camera.step(
                    input(1.0f / 60.0f, focus, true, zoom));
                require(edge_violation(output) <= 2.0e-6f,
                        "activation exposed invalid source pixels");

                const Vec2 focus_output = cursor_output_position(
                    focus, output.center, output.zoom);
                require(focus_output.x >= -1.0e-4f &&
                            focus_output.x <= 1.0001f &&
                            focus_output.y >= -1.0e-4f &&
                            focus_output.y <= 1.0001f,
                        "latched activation focus left the visible output");

                if (length(direction) > 0.0f) {
                    const float projection = dot(
                        sub(output.center, Vec2{0.5f, 0.5f}), direction);
                    require(projection + 2.0e-5f >= previous_projection,
                            "activation camera center detoured away from focus");
                    previous_projection = projection;
                }
            }

            const CameraOutput final_output = camera.output();
            require(std::fabs(final_output.zoom - zoom) < 0.01f,
                    "activation did not reach requested zoom");
            require(length(sub(final_output.center, final_center)) < 0.01f,
                    "activation did not settle on the legal focus framing");
        }
    }
}

void test_activation_latches_focus()
{
    using namespace arzoom;

    SmartCamera camera;
    const Vec2 activation_focus{0.92f, 0.24f};
    camera.step(input(1.0f / 60.0f, activation_focus, false, 3.0f));
    camera.step(input(1.0f / 60.0f, activation_focus, true, 3.0f));

    for (int frame = 0; frame < 45; ++frame) {
        const float jitter = 0.02f * std::sin(static_cast<float>(frame));
        const Vec2 moving_cursor{0.55f + jitter, 0.55f - jitter};
        camera.step(input(1.0f / 60.0f, moving_cursor, true, 3.0f));
    }

    const Vec2 expected = smart_follow_target(
        activation_focus, {0.5f, 0.5f}, {0.5f, 0.45f}, 0.28f, 3.0f);
    require(length(sub(camera.output().center, expected)) < 0.015f,
            "ordinary mouse movement retargeted focus during activation");
}

void test_viewer_comfort()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);
    const Vec2 initial = camera.output().center;
    float max_jitter_displacement = 0.0f;

    for (int i = 0; i < 120; ++i) {
        const float t = static_cast<float>(i);
        const Vec2 cursor{
            0.5f + 0.015f * std::sin(t * 0.80f),
            0.5f + 0.015f * std::cos(t * 1.10f),
        };
        const CameraOutput output = camera.step(
            input(1.0f / 60.0f, cursor, true, 2.0f));
        max_jitter_displacement = std::max(
            max_jitter_displacement,
            length(sub(output.center, initial)));
    }
    require(max_jitter_displacement < 1.0e-5f,
            "normal hand jitter moved the Smart Camera");

    camera.reset();
    warm_center(camera);
    const Vec2 circle_start = camera.output().center;
    float max_circle_displacement = 0.0f;
    for (int i = 0; i < 120; ++i) {
        const float phase = 6.28318530718f *
                            static_cast<float>(i) / 120.0f;
        const Vec2 cursor{
            0.5f + 0.05f * std::cos(phase),
            0.5f + 0.05f * std::sin(phase),
        };
        const CameraOutput output = camera.step(
            input(1.0f / 60.0f, cursor, true, 2.0f));
        max_circle_displacement = std::max(
            max_circle_displacement,
            length(sub(output.center, circle_start)));
    }
    require(max_circle_displacement < 1.0e-5f,
            "small explanatory circle should not create camera wander");
}

void test_ballistic_long_relocation()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);

    int first_motion_frame = -1;
    float first_motion_speed = 0.0f;
    float max_output_speed = 0.0f;
    float previous_output_acceleration = 0.0f;
    float max_jerk = 0.0f;

    for (int frame = 0; frame < 120; ++frame) {
        const CameraOutput output = camera.step(
            input(1.0f / 60.0f, {0.90f, 0.50f}, true, 2.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "ballistic relocation exposed an invalid edge");

        const float output_speed = length(output.velocity) * output.zoom;
        const float output_acceleration =
            length(output.acceleration) * output.zoom;
        max_output_speed = std::max(max_output_speed, output_speed);
        max_jerk = std::max(
            max_jerk,
            std::fabs(output_acceleration - previous_output_acceleration) /
                (1.0f / 60.0f));
        previous_output_acceleration = output_acceleration;

        if (first_motion_frame < 0 && output_speed > 0.001f) {
            first_motion_frame = frame;
            first_motion_speed = output_speed;
        }
    }

    const CameraOutput final_output = camera.output();
    require(first_motion_frame >= 1,
            "intent-driven camera moved immediately without observation");
    require(first_motion_speed < 0.25f,
            "camera launched at robotic near-max speed instead of accelerating");
    require(max_output_speed < 2.70f,
            "urgency-scaled ballistic speed exceeded the designed bound");
    require(max_jerk < 80.0f,
            "camera jerk exceeded the designed smoothness envelope");
    require(final_output.center.x > 0.745f,
            "long relocation did not reacquire the legal right-side framing");
    require(length(final_output.velocity) * final_output.zoom < 0.001f,
            "camera did not fully settle after relocation");
}

void test_frame_rate_consistency()
{
    using namespace arzoom;

    const int fps_values[] = {30, 60, 120, 144};
    Vec2 final_centers[4];
    float final_zooms[4] = {};

    for (int index = 0; index < 4; ++index) {
        const int fps = fps_values[index];
        SmartCamera camera;
        camera.step(input(1.0f / fps, {0.5f, 0.5f}, false, 2.0f));
        for (int frame = 0; frame < fps; ++frame)
            camera.step(input(1.0f / fps, {0.5f, 0.5f}, true, 2.0f));
        for (int frame = 0; frame < 2 * fps; ++frame)
            camera.step(input(1.0f / fps, {0.90f, 0.50f}, true, 2.0f));
        final_centers[index] = camera.output().center;
        final_zooms[index] = camera.output().zoom;
    }

    for (int index = 1; index < 4; ++index) {
        require(length(sub(final_centers[0], final_centers[index])) < 0.004f,
                "30/60/120/144 fps Smart Camera framing diverged");
        require(std::fabs(final_zooms[0] - final_zooms[index]) < 0.004f,
                "30/60/120/144 fps Smart Camera zoom diverged");
    }
}

void test_zoom_out_edge_safety()
{
    using namespace arzoom;

    SmartCamera camera;
    camera.step(input(1.0f / 60.0f, {0.95f, 0.50f}, false, 3.0f));
    for (int frame = 0; frame < 120; ++frame)
        camera.step(input(1.0f / 60.0f, {0.95f, 0.50f}, true, 3.0f));

    for (int frame = 0; frame < 180; ++frame) {
        const CameraOutput output = camera.step(
            input(1.0f / 60.0f, {0.95f, 0.50f}, false, 3.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "ballistic return/zoom-out exposed invalid source pixels");
    }

    const CameraOutput final_output = camera.output();
    require(std::fabs(final_output.zoom - 1.0f) < 0.01f,
            "ballistic return did not reach 1x");
    require(length(sub(final_output.center, Vec2{0.5f, 0.5f})) < 0.01f,
            "ballistic return did not settle at full-frame center");
}

} // namespace

int main()
{
    test_activation_focus_continuity();
    test_activation_latches_focus();
    test_viewer_comfort();
    test_ballistic_long_relocation();
    test_frame_rate_consistency();
    test_zoom_out_edge_safety();

    std::cout << "ArZoom Smart Camera Motion 2.0 tests: PASS\n";
    return 0;
}
