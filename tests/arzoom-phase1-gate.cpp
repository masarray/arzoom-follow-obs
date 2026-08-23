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

arzoom::Vec2 screen_pos(arzoom::Vec2 source,
                        const arzoom::CameraOutput &camera)
{
    return arzoom::cursor_output_position(source, camera.center, camera.zoom);
}

float distance_from_line(arzoom::Vec2 p, arzoom::Vec2 a, arzoom::Vec2 b)
{
    const arzoom::Vec2 ab = arzoom::sub(b, a);
    const float len = arzoom::length(ab);
    if (len < 1.0e-7f)
        return arzoom::length(arzoom::sub(p, a));
    const arzoom::Vec2 ap = arzoom::sub(p, a);
    return std::fabs(ap.x * ab.y - ap.y * ab.x) / len;
}

float line_progress(arzoom::Vec2 p, arzoom::Vec2 a, arzoom::Vec2 b)
{
    const arzoom::Vec2 ab = arzoom::sub(b, a);
    const float denom = ab.x * ab.x + ab.y * ab.y;
    if (denom < 1.0e-9f)
        return 1.0f;
    const arzoom::Vec2 ap = arzoom::sub(p, a);
    return (ap.x * ab.x + ap.y * ab.y) / denom;
}

void warm_center(arzoom::SmartCamera &camera, int fps = 60,
                 float zoom = 2.0f)
{
    camera.step(sample(1.0f / fps, {0.5f, 0.5f}, false, zoom));
    for (int i = 0; i < fps; ++i)
        camera.step(sample(1.0f / fps, {0.5f, 0.5f}, true, zoom));
}

void straight_zoom_in_screen_path()
{
    using namespace arzoom;
    const Vec2 focus{0.92f, 0.22f};
    const Vec2 probes[] = {{0.15f, 0.20f}, {0.72f, 0.66f}, {0.94f, 0.12f}};
    const float target_zoom = 3.0f;
    const Vec2 final_center = smart_follow_target(
        focus, {0.5f, 0.5f}, {0.5f, 0.45f}, 0.28f, target_zoom);

    SmartCamera camera;
    camera.step(sample(1.0f / 60.0f, focus, false, target_zoom));

    float previous_progress[3] = {-0.001f, -0.001f, -0.001f};
    for (int frame = 0; frame < 90; ++frame) {
        const CameraOutput out = camera.step(
            sample(1.0f / 60.0f, focus, true, target_zoom));
        require(edge_violation(out) <= 2.0e-6f,
                "zoom-in exposed invalid source pixels");

        for (int i = 0; i < 3; ++i) {
            const Vec2 start = probes[i];
            const Vec2 finish = cursor_output_position(
                probes[i], final_center, target_zoom);
            const Vec2 current = screen_pos(probes[i], out);
            require(distance_from_line(current, start, finish) < 2.0e-4f,
                    "zoom-in pixel trajectory curved in screen space");
            const float p = line_progress(current, start, finish);
            require(p + 2.0e-4f >= previous_progress[i],
                    "zoom-in reversed direction before the target");
            previous_progress[i] = p;
        }
    }
}

void explanation_circle_stays_locked()
{
    using namespace arzoom;
    SmartCamera camera;
    warm_center(camera);
    const Vec2 start_center = camera.output().center;
    float max_displacement = 0.0f;

    for (int i = 0; i < 240; ++i) {
        const float phase = 6.28318530718f * static_cast<float>(i) / 60.0f;
        const Vec2 cursor{
            0.5f + 0.075f * std::cos(phase),
            0.5f + 0.075f * std::sin(phase),
        };
        const CameraOutput out = camera.step(
            sample(1.0f / 60.0f, cursor, true, 2.0f));
        max_displacement = std::max(
            max_displacement, length(sub(out.center, start_center)));
    }

    require(max_displacement < 1.0e-5f,
            "local explanation circle moved the viewport");
}

void real_relocation_still_follows()
{
    using namespace arzoom;
    SmartCamera camera;
    warm_center(camera);
    const Vec2 start = camera.output().center;

    for (int frame = 0; frame < 180; ++frame)
        camera.step(sample(1.0f / 60.0f, {0.90f, 0.50f}, true, 2.0f));

    const CameraOutput out = camera.output();
    require(out.center.x - start.x > 0.20f,
            "explanation lock blocked a real relocation");
    require(edge_violation(out) <= 2.0e-6f,
            "real relocation violated source edges");
}

void continuous_retarget_has_no_snap()
{
    using namespace arzoom;
    SmartCamera camera;
    warm_center(camera);

    float previous_x = camera.output().center.x;
    float max_step = 0.0f;
    for (int frame = 0; frame < 150; ++frame) {
        const Vec2 cursor = frame < 45 ? Vec2{0.90f, 0.50f}
                                      : Vec2{0.18f, 0.62f};
        const CameraOutput out = camera.step(
            sample(1.0f / 60.0f, cursor, true, 2.0f));
        max_step = std::max(max_step, std::fabs(out.center.x - previous_x));
        previous_x = out.center.x;
    }
    require(max_step < 0.030f,
            "mid-flight retarget produced a visible camera snap");
}

void straight_zoom_out_screen_path()
{
    using namespace arzoom;
    const Vec2 focus{0.92f, 0.20f};
    SmartCamera camera;
    camera.step(sample(1.0f / 60.0f, focus, false, 3.0f));
    for (int frame = 0; frame < 90; ++frame)
        camera.step(sample(1.0f / 60.0f, focus, true, 3.0f));

    const CameraOutput start_camera = camera.output();
    const Vec2 probes[] = {{0.10f, 0.18f}, {0.62f, 0.72f}, {0.95f, 0.10f}};
    Vec2 starts[3];
    float previous_progress[3] = {-0.001f, -0.001f, -0.001f};
    for (int i = 0; i < 3; ++i)
        starts[i] = screen_pos(probes[i], start_camera);

    float previous_zoom = start_camera.zoom + 0.001f;
    for (int frame = 0; frame < 120; ++frame) {
        const CameraOutput out = camera.step(
            sample(1.0f / 60.0f, focus, false, 3.0f));
        require(edge_violation(out) <= 2.0e-6f,
                "zoom-out exposed invalid source pixels");
        require(out.zoom <= previous_zoom + 2.0e-5f,
                "zoom-out reversed magnification direction");
        previous_zoom = out.zoom;

        for (int i = 0; i < 3; ++i) {
            const Vec2 finish = probes[i];
            const Vec2 current = screen_pos(probes[i], out);
            require(distance_from_line(current, starts[i], finish) < 2.5e-4f,
                    "zoom-out pixel trajectory curved in screen space");
            const float p = line_progress(current, starts[i], finish);
            require(p + 2.0e-4f >= previous_progress[i],
                    "zoom-out bent/reversed before final position");
            previous_progress[i] = p;
        }
    }

    const CameraOutput settled = camera.output();
    require(std::fabs(settled.zoom - 1.0f) < 1.0e-6f,
            "zoom-out did not exact-lock to 1x");
    require(length(sub(settled.center, Vec2{0.5f, 0.5f})) < 1.0e-6f,
            "zoom-out did not exact-lock to center");

    for (int frame = 0; frame < 120; ++frame) {
        const CameraOutput out = camera.step(
            sample(1.0f / 60.0f, focus, false, 3.0f));
        require(length(sub(out.center, Vec2{0.5f, 0.5f})) < 1.0e-7f,
                "post zoom-out viewport breathed after settle");
    }
}

void frame_rate_consistency()
{
    using namespace arzoom;
    const int fps_values[] = {30, 60, 120, 144};
    Vec2 centers[4];

    for (int index = 0; index < 4; ++index) {
        const int fps = fps_values[index];
        SmartCamera camera;
        camera.step(sample(1.0f / fps, {0.5f, 0.5f}, false, 2.0f));
        for (int frame = 0; frame < fps; ++frame)
            camera.step(sample(1.0f / fps, {0.5f, 0.5f}, true, 2.0f));
        for (int frame = 0; frame < 3 * fps; ++frame)
            camera.step(sample(1.0f / fps, {0.90f, 0.50f}, true, 2.0f));
        centers[index] = camera.output().center;
    }

    for (int index = 1; index < 4; ++index) {
        require(length(sub(centers[0], centers[index])) < 0.005f,
                "30/60/120/144 fps gimbal framing diverged");
    }
}

} // namespace

int main()
{
    straight_zoom_in_screen_path();
    explanation_circle_stays_locked();
    real_relocation_still_follows();
    continuous_retarget_has_no_snap();
    straight_zoom_out_screen_path();
    frame_rate_consistency();
    std::cout << "ArZoom Smart Gimbal straight-path gates: PASS\n";
    return 0;
}
