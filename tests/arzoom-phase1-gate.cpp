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
                        "latched activation focus left the visible output");

                if (length(direction) > 0.0f) {
                    const float projection = dot(
                        sub(output.center, Vec2{0.5f, 0.5f}), direction);
                    require(projection + 2.0e-5f >= previous_projection,
                            "activation detoured away from the requested focus");
                    previous_projection = projection;
                }
            }

            const CameraOutput final_output = camera.output();
            require(std::fabs(final_output.zoom - zoom) < 0.01f,
                    "activation did not reach requested zoom");
            require(length(sub(final_output.center, expected)) < 0.01f,
                    "activation did not settle on legal focus framing");
        }
    }
}

void activation_latches_only_until_handoff()
{
    using namespace arzoom;

    SmartCamera camera;
    const Vec2 activation_focus{0.92f, 0.24f};
    const Vec2 new_cursor{0.55f, 0.55f};
    const float zoom = 3.0f;

    camera.step(sample(1.0f / 60.0f, activation_focus, false, zoom));
    camera.step(sample(1.0f / 60.0f, activation_focus, true, zoom));

    const Vec2 expected = smart_follow_target(
        activation_focus, {0.5f, 0.5f}, {0.5f, 0.45f}, 0.28f, zoom);
    const Vec2 direction = normalized(
        sub(expected, Vec2{0.5f, 0.5f}));

    bool saw_activation = false;
    bool saw_handoff = false;
    float previous_projection = -1.0e-5f;
    CameraOutput handoff{};

    for (int frame = 0; frame < 90; ++frame) {
        const float jitter = 0.02f * std::sin(static_cast<float>(frame));
        const Vec2 moving_cursor{new_cursor.x + jitter,
                                 new_cursor.y - jitter};
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, moving_cursor, true, zoom));

        if (output.state == CameraState::Activating) {
            saw_activation = true;
            const float projection = dot(
                sub(output.center, Vec2{0.5f, 0.5f}), direction);
            require(projection + 2.0e-5f >= previous_projection,
                    "cursor motion retargeted camera during activation latch");
            previous_projection = projection;
            continue;
        }

        if (saw_activation) {
            handoff = output;
            saw_handoff = true;
            break;
        }
    }

    require(saw_activation, "activation latch state was never observed");
    require(saw_handoff, "activation never handed off to Smart Follow");
    require(length(sub(handoff.center, expected)) < 0.016f,
            "camera handed off before reaching latched activation focus");

    /* After activation ends, a real new cursor relocation must be allowed to
     * become presenter intent. The latch is not a permanent cursor lock. */
    const Vec2 before_follow = camera.output().center;
    for (int frame = 0; frame < 120; ++frame)
        camera.step(sample(1.0f / 60.0f, new_cursor, true, zoom));
    const Vec2 after_follow = camera.output().center;
    require(length(sub(after_follow, before_follow)) > 0.02f,
            "Smart Follow remained permanently latched after activation");
}

void viewer_comfort()
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
            "normal hand jitter moved the Smart Camera");

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
            "small explanatory circle created camera wander");
}

void ballistic_long_relocation()
{
    using namespace arzoom;

    SmartCamera camera;
    warm_center(camera);
    int first_motion_frame = -1;
    float first_motion_speed = 0.0f;
    float max_output_speed = 0.0f;

    for (int frame = 0; frame < 120; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, {0.90f, 0.50f}, true, 2.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "ballistic relocation exposed invalid source pixels");
        const float speed = length(output.velocity) * output.zoom;
        max_output_speed = std::max(max_output_speed, speed);
        if (first_motion_frame < 0 && speed > 0.001f) {
            first_motion_frame = frame;
            first_motion_speed = speed;
        }
    }

    const CameraOutput final_output = camera.output();
    require(first_motion_frame >= 1,
            "camera skipped intent observation and moved instantly");
    require(first_motion_speed < 0.25f,
            "camera launched at robotic near-max speed");
    require(max_output_speed < 2.70f,
            "urgency-scaled speed exceeded designed bound");
    require(final_output.center.x > 0.745f,
            "long relocation did not reacquire right-side framing");
    require(length(final_output.velocity) * final_output.zoom < 0.001f,
            "camera did not fully settle after long relocation");
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
        for (int frame = 0; frame < 2 * fps; ++frame)
            camera.step(sample(1.0f / fps, {0.90f, 0.50f}, true, 2.0f));
        centers[index] = camera.output().center;
        zooms[index] = camera.output().zoom;
    }

    for (int index = 1; index < 4; ++index) {
        require(length(sub(centers[0], centers[index])) < 0.004f,
                "30/60/120/144 fps framing diverged");
        require(std::fabs(zooms[0] - zooms[index]) < 0.004f,
                "30/60/120/144 fps zoom diverged");
    }
}

void zoom_out_edge_safety()
{
    using namespace arzoom;
    SmartCamera camera;
    camera.step(sample(1.0f / 60.0f, {0.95f, 0.50f}, false, 3.0f));
    for (int frame = 0; frame < 120; ++frame)
        camera.step(sample(1.0f / 60.0f, {0.95f, 0.50f}, true, 3.0f));

    for (int frame = 0; frame < 180; ++frame) {
        const CameraOutput output = camera.step(
            sample(1.0f / 60.0f, {0.95f, 0.50f}, false, 3.0f));
        require(edge_violation(output) <= 2.0e-6f,
                "return/zoom-out exposed invalid source pixels");
    }

    const CameraOutput final_output = camera.output();
    require(std::fabs(final_output.zoom - 1.0f) < 0.01f,
            "return did not reach 1x");
    require(length(sub(final_output.center, Vec2{0.5f, 0.5f})) < 0.01f,
            "return did not settle at full-frame center");
}

} // namespace

int main()
{
    activation_focus_continuity();
    activation_latches_only_until_handoff();
    viewer_comfort();
    ballistic_long_relocation();
    frame_rate_consistency();
    zoom_out_edge_safety();
    std::cout << "ArZoom Smart Camera Motion 2.0 gates: PASS\n";
    return 0;
}
