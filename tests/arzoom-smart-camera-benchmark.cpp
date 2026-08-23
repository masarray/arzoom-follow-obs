#include "../src/arzoom-camera.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;

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

template <typename StepFn>
void run_case(const std::string &name, std::uint64_t iterations, StepFn step)
{
    volatile float sink = 0.0f;
    const auto start = Clock::now();
    for (std::uint64_t i = 0; i < iterations; ++i)
        sink += step(i);
    const auto end = Clock::now();

    const double total_ns = std::chrono::duration<double, std::nano>(
        end - start).count();
    const double ns_per_update = total_ns / static_cast<double>(iterations);
    const double updates_per_second = 1.0e9 / ns_per_update;

    std::cout << std::left << std::setw(28) << name
              << " ns/update=" << std::fixed << std::setprecision(2)
              << ns_per_update
              << " updates/s=" << std::setprecision(0)
              << updates_per_second
              << " sink=" << std::setprecision(4) << sink << '\n';
}

} // namespace

int main()
{
    using namespace arzoom;
    constexpr std::uint64_t iterations = 1000000;
    constexpr float dt = 1.0f / 60.0f;

    {
        SmartCamera camera;
        camera.step(sample(dt, {0.5f, 0.5f}, false));
        for (int i = 0; i < 60; ++i)
            camera.step(sample(dt, {0.5f, 0.5f}, true));

        run_case("smart-camera/jitter", iterations,
                 [&](std::uint64_t i) {
                     const float t = static_cast<float>(i & 1023u);
                     const Vec2 cursor{
                         0.5f + 0.015f * std::sin(t * 0.80f),
                         0.5f + 0.015f * std::cos(t * 1.10f),
                     };
                     const CameraOutput out = camera.step(
                         sample(dt, cursor, true, 2.0f));
                     return out.center.x + out.center.y + out.zoom;
                 });
    }

    {
        SmartCamera camera;
        camera.step(sample(dt, {0.5f, 0.5f}, false));
        for (int i = 0; i < 60; ++i)
            camera.step(sample(dt, {0.5f, 0.5f}, true));

        run_case("smart-camera/catch-up", iterations,
                 [&](std::uint64_t i) {
                     const bool right = ((i / 120u) & 1u) == 0u;
                     const Vec2 cursor = right
                                             ? Vec2{0.92f, 0.24f}
                                             : Vec2{0.08f, 0.76f};
                     const CameraOutput out = camera.step(
                         sample(dt, cursor, true, 2.0f));
                     return out.center.x + out.center.y +
                            out.velocity.x + out.velocity.y;
                 });
    }

    {
        SmartCamera camera;
        run_case("smart-camera/activation", iterations,
                 [&](std::uint64_t i) {
                     const std::uint64_t phase = i % 90u;
                     const bool zoomed = phase >= 1u && phase < 60u;
                     const Vec2 cursor{0.92f, 0.18f};
                     const CameraOutput out = camera.step(
                         sample(dt, cursor, zoomed, 3.0f));
                     return out.center.x + out.center.y + out.zoom;
                 });
    }

    std::cout << "Smart Camera benchmark completed. Absolute timings are "
                 "runner-specific engineering diagnostics.\n";
    return 0;
}
