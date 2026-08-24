#include "../src/arzoom-presenter-controls.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

namespace {

using Clock = std::chrono::steady_clock;

void report(const char *name, std::uint64_t iterations,
            const std::function<void(std::uint64_t)> &fn)
{
    const auto start = Clock::now();
    fn(iterations);
    const auto end = Clock::now();
    const double ns = std::chrono::duration<double, std::nano>(end - start).count();
    std::cout << name << " ns/update=" << (ns / static_cast<double>(iterations))
              << '\n';
}

} // namespace

int main()
{
    constexpr std::uint64_t iterations = 1000000;
    volatile float sink = 0.0f;

    report("presenter/idle-helpers", iterations,
           [&](std::uint64_t count) {
               bool latched = false;
               bool held = false;
               float zoom = 2.0f;
               for (std::uint64_t i = 0; i < count; ++i) {
                   held = (i & 31u) == 0u;
                   sink += arzoom::presenter_zoom_requested(latched, held)
                               ? 1.0f
                               : 0.0f;
                   zoom = arzoom::presenter_zoom_step(
                       zoom, (i & 1u) ? 0.25f : -0.25f);
                   sink += zoom * 0.000001f;
               }
           });

    report("presenter/overview-active", iterations,
           [&](std::uint64_t count) {
               arzoom::OverviewPeekController controller;
               for (std::uint64_t i = 0; i < count; ++i) {
                   if (!controller.active())
                       controller.begin({0.68f, 0.36f}, 2.75f);
                   const arzoom::OverviewOutput output =
                       controller.step(1.0f / 144.0f, 0.34f, 0.32f);
                   sink += output.center.x * 0.000001f +
                           output.zoom * 0.000001f;
                   if (output.phase == arzoom::OverviewPhase::Holding)
                       controller.release(output.center, output.zoom);
               }
           });

    if (sink < -1.0f)
        std::cerr << sink << '\n';
    return 0;
}
