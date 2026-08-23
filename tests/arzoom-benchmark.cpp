#include "arzoom-trace-harness.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

template <typename Factory>
void run_benchmark(const char *name, Factory factory, int iterations)
{
    using clock = std::chrono::steady_clock;
    std::uint64_t samples = 0;
    volatile float sink = 0.0f;

    const auto trace = factory();
    const auto start = clock::now();
    for (int i = 0; i < iterations; ++i) {
        arzoom::phase0::BaselineCamera camera;
        for (const auto &sample : trace) {
            const auto snap = camera.step(sample);
            sink = sink + snap.center.x + snap.center.y + snap.zoom;
            ++samples;
        }
    }
    const auto end = clock::now();
    const double ns = std::chrono::duration<double, std::nano>(end - start).count();
    const double ns_per_update = ns / static_cast<double>(samples);
    const double updates_per_second = 1.0e9 / ns_per_update;

    std::cout << std::left << std::setw(24) << name
              << " updates=" << samples
              << " ns/update=" << std::fixed << std::setprecision(2) << ns_per_update
              << " updates/s=" << std::setprecision(0) << updates_per_second
              << " sink=" << sink << '\n';
}

} // namespace

int main()
{
    constexpr int iterations = 2500;
    std::cout << "ArZoom Phase 0 motion microbenchmark\n";
    std::cout << "Directional regression signal only; do not use as a cross-machine marketing claim.\n";

    run_benchmark("jitter", [] { return arzoom::phase0::jitter_trace(); }, iterations);
    run_benchmark("long_relocation", [] { return arzoom::phase0::long_relocation_trace(); }, iterations);
    run_benchmark("edge_travel", [] { return arzoom::phase0::edge_travel_trace(); }, iterations);
    run_benchmark("zoom_out_near_edge", [] { return arzoom::phase0::zoom_out_near_edge_trace(); }, iterations);
    return 0;
}
