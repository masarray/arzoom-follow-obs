#include "../src/arzoom-click-visual.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

using Clock = std::chrono::steady_clock;

volatile float sink = 0.0f;

template <typename Fn>
void run_case(const char *name, Fn fn)
{
    constexpr uint64_t iterations = 2000000;
    const auto start = Clock::now();
    for (uint64_t i = 0; i < iterations; ++i)
        fn(i);
    const auto finish = Clock::now();
    const double ns = std::chrono::duration<double, std::nano>(
        finish - start).count();
    const double per_update = ns / static_cast<double>(iterations);
    std::cout << std::left << std::setw(24) << name << ": "
              << std::fixed << std::setprecision(2) << per_update
              << " ns/update\n";
}

} // namespace

int main()
{
    using namespace arzoom;

    ClickVisualState idle;
    run_case("click idle", [&](uint64_t) {
        idle.advance(1.0f / 60.0f);
        sink += static_cast<float>(idle.active_count());
    });

    ClickVisualState single;
    single.push(ClickType::Left, {0.5f, 0.5f});
    run_case("one active click", [&](uint64_t i) {
        single.advance(0.000001f);
        if (!single.has_active())
            single.push(ClickType::Left, {0.5f, 0.5f});
        sink += single.slot(static_cast<size_t>(i) %
                            ClickVisualState::kSlotCount).age_seconds;
    });

    ClickVisualState four;
    four.push(ClickType::Left, {0.2f, 0.2f});
    four.push(ClickType::Right, {0.4f, 0.4f});
    four.push(ClickType::Middle, {0.6f, 0.6f});
    four.push(ClickType::Left, {0.8f, 0.8f});
    run_case("four active clicks", [&](uint64_t i) {
        four.advance(0.000001f);
        if (!four.has_active()) {
            four.push(ClickType::Left, {0.2f, 0.2f});
            four.push(ClickType::Right, {0.4f, 0.4f});
            four.push(ClickType::Middle, {0.6f, 0.6f});
            four.push(ClickType::Left, {0.8f, 0.8f});
        }
        sink += four.slot(static_cast<size_t>(i) %
                          ClickVisualState::kSlotCount).age_seconds;
    });

    return sink < -1.0f ? 1 : 0;
}
