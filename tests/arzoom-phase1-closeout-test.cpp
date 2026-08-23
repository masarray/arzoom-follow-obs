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
                           bool zoom_requested, float zoom)
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

void warm_zoom(arzoom::SmartCamera &camera, int fps, float zoom,
               arzoom::Vec2 focus = {0.5f, 0.5f})
{
    camera.step(sample(1.0f / fps, focus, false, zoom));
    for (int i = 0; i < 2 * fps; ++i) {
        const auto out = camera.step(sample(1.0f / fps, focus, true, zoom));
        require(edge_violation(out) <= 2.0e-6f,
                "warm zoom exposed invalid source pixels");
    }
    require(std::fabs(camera.output().zoom - zoom) < 0.01f,
            "warm zoom did not reach configured magnification");
}

void smart_zone_matrix()
{
    using namespace arzoom;
    const int fps_values[] = {30, 60, 120, 144};
    const float zooms[] = {2.0f, 3.0f, 4.0f};
    const Vec2 zones[] = {
        {0.82f, 0.22f}, {0.18f, 0.78f}, {0.82f, 0.78f},
        {0.18f, 0.22f}, {0.50f, 0.50f},
    };

    for (int fps : fps_values) {
        const float dt = 1.0f / static_cast<float>(fps);
        for (float zoom : zooms) {
            SmartCamera camera;
            warm_zoom(camera, fps, zoom);

            for (const Vec2 zone : zones) {
                for (int frame = 0; frame < 3 * fps; ++frame) {
                    const auto out = camera.step(sample(dt, zone, true, zoom));
                    require(edge_violation(out) <= 2.0e-6f,
                            "Smart Zone relocation violated viewport edges");
                }

                require(camera.output().state == CameraState::SmoothIdle,
                        "Smart Zone did not settle to SmoothIdle");

                const Vec2 idle_center = camera.output().center;
                float max_idle_displacement = 0.0f;
                const float orbit_radius = 0.075f / zoom;
                for (int frame = 0; frame < fps; ++frame) {
                    const float phase = 6.28318530718f *
                        static_cast<float>(frame) /
                        static_cast<float>(fps);
                    const Vec2 cursor{
                        zone.x + orbit_radius * std::cos(phase),
                        zone.y + orbit_radius * std::sin(phase),
                    };
                    const auto out = camera.step(sample(dt, cursor, true, zoom));
                    require(edge_violation(out) <= 2.0e-6f,
                            "SmoothIdle explanation orbit violated viewport edges");
                    max_idle_displacement = std::max(
                        max_idle_displacement,
                        length(sub(out.center, idle_center)));
                }
                require(max_idle_displacement < 1.0e-5f,
                        "local explanation orbit moved a settled Smart Zone");
            }
        }
    }
}

void rapid_zone_switching_stays_bounded()
{
    using namespace arzoom;
    constexpr int fps = 60;
    constexpr float zoom = 3.0f;
    constexpr float dt = 1.0f / fps;
    SmartCamera camera;
    warm_zoom(camera, fps, zoom);

    const Vec2 zones[] = {
        {0.16f, 0.22f}, {0.84f, 0.76f},
        {0.18f, 0.78f}, {0.82f, 0.20f},
    };

    Vec2 previous = camera.output().center;
    float max_step_output = 0.0f;
    for (int frame = 0; frame < 12 * fps; ++frame) {
        const int zone_index = (frame / 21) % 4;
        const auto out = camera.step(
            sample(dt, zones[zone_index], true, zoom));
        require(std::isfinite(out.center.x) && std::isfinite(out.center.y) &&
                    std::isfinite(out.zoom),
                "rapid zone switching produced non-finite camera output");
        require(edge_violation(out) <= 2.0e-6f,
                "rapid zone switching violated viewport edges");
        require(std::fabs(out.zoom - zoom) < 0.02f,
                "rapid zone switching changed zoom unexpectedly");
        max_step_output = std::max(
            max_step_output,
            length(sub(out.center, previous)) * out.zoom);
        previous = out.center;
    }

    require(max_step_output < 0.060f,
            "rapid zone switching produced a one-frame viewport snap");
}

void corner_zoomout_matrix()
{
    using namespace arzoom;
    constexpr int fps = 60;
    constexpr float dt = 1.0f / fps;
    const float zooms[] = {2.0f, 3.0f, 4.0f};
    const Vec2 corners[] = {
        {0.04f, 0.04f}, {0.96f, 0.04f},
        {0.04f, 0.96f}, {0.96f, 0.96f},
    };

    for (float zoom : zooms) {
        for (const Vec2 focus : corners) {
            SmartCamera camera;
            warm_zoom(camera, fps, zoom, focus);

            float previous_zoom = camera.output().zoom + 0.001f;
            for (int frame = 0; frame < 2 * fps; ++frame) {
                const auto out = camera.step(sample(dt, focus, false, zoom));
                require(edge_violation(out) <= 2.0e-6f,
                        "corner zoom-out violated viewport edges");
                require(out.zoom <= previous_zoom + 2.0e-5f,
                        "corner zoom-out reversed magnification direction");
                previous_zoom = out.zoom;
            }

            const auto settled = camera.output();
            require(std::fabs(settled.zoom - 1.0f) < 1.0e-6f,
                    "corner zoom-out did not finish at exact 1x");
            require(length(sub(settled.center, Vec2{0.5f, 0.5f})) < 1.0e-6f,
                    "corner zoom-out did not finish at exact center");

            for (int frame = 0; frame < fps; ++frame) {
                const auto out = camera.step(sample(dt, focus, false, zoom));
                require(std::fabs(out.zoom - 1.0f) < 1.0e-7f &&
                            length(sub(out.center, Vec2{0.5f, 0.5f})) < 1.0e-7f,
                        "full-frame camera breathed after corner zoom-out");
            }
        }
    }
}

} // namespace

int main()
{
    smart_zone_matrix();
    rapid_zone_switching_stays_bounded();
    corner_zoomout_matrix();
    std::cout << "ArZoom Phase 1 closeout stress matrix: PASS\n";
    return 0;
}
