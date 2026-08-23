#include "arzoom-trace-harness.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using arzoom::phase0::TraceMetrics;

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

void require_edge_safe(const std::string &name, const TraceMetrics &m)
{
    require(m.max_edge_violation <= 1.0e-6f, name + ": viewport exposed invalid source area");
    require(m.max_apparent_speed <= 1.3505f, name + ": apparent pan speed exceeded baseline limit");
}

void print_metrics(const std::string &name, const TraceMetrics &m)
{
    std::cout << name
              << " displacement=" << m.max_center_displacement
              << " max_speed=" << m.max_apparent_speed
              << " move_frames=" << m.camera_move_frames
              << " edge_violation=" << m.max_edge_violation
              << " final_center=(" << m.final.center.x << ',' << m.final.center.y << ')'
              << " final_zoom=" << m.final.zoom
              << '\n';
}

} // namespace

int main()
{
    using namespace arzoom::phase0;

    const auto jitter = replay(jitter_trace());
    print_metrics("jitter", jitter);
    require_edge_safe("jitter", jitter);
    require(jitter.max_center_displacement <= 1.0e-6f,
            "normal hand jitter should not move the baseline camera");

    const auto circle = replay(explanation_circle_trace());
    print_metrics("explanation_circle", circle);
    require_edge_safe("explanation_circle", circle);
    require(circle.max_center_displacement < 0.006f,
            "small explanatory gesture should keep framing essentially stable");

    const auto pointing = replay(repeated_pointing_trace());
    print_metrics("repeated_pointing", pointing);
    require_edge_safe("repeated_pointing", pointing);
    require(pointing.max_center_displacement < 0.012f,
            "repeated pointing inside one UI region should not create large framing motion");

    const auto nearby = replay(nearby_deliberate_trace());
    print_metrics("nearby_deliberate", nearby);
    require_edge_safe("nearby_deliberate", nearby);
    require(nearby.final.center.x > 0.595f && nearby.final.center.x < 0.615f,
            "slow deliberate nearby travel should produce a bounded follow correction");

    const auto relocation = replay(long_relocation_trace());
    print_metrics("long_relocation", relocation);
    require_edge_safe("long_relocation", relocation);
    require(relocation.final.center.x > 0.745f && relocation.final.center.x <= 0.7501f,
            "long relocation should reach the legal right-side framing target");

    const auto reversal = replay(direction_reversal_trace());
    print_metrics("direction_reversal", reversal);
    require_edge_safe("direction_reversal", reversal);
    require(reversal.final.center.x < 0.255f,
            "direction reversal should eventually reacquire the opposite target");

    const auto edge = replay(edge_travel_trace());
    print_metrics("edge_travel", edge);
    require_edge_safe("edge_travel", edge);

    const auto corner = replay(corner_trace());
    print_metrics("corner", corner);
    require_edge_safe("corner", corner);
    require(corner.final.center.x <= 0.7501f && corner.final.center.y <= 0.7501f,
            "corner target must remain clamped at 2x zoom");

    const auto click = replay(click_after_relocation_trace());
    print_metrics("click_after_relocation", click);
    require_edge_safe("click_after_relocation", click);
    require(click.click_count == 1, "left click event should be preserved by the trace harness");

    const auto rapid = replay(rapid_click_trace());
    print_metrics("rapid_clicks", rapid);
    require_edge_safe("rapid_clicks", rapid);
    require(rapid.click_count == 5, "rapid click sequence should preserve all presentation events");

    const auto zoom_move = replay(zoom_while_moving_trace());
    print_metrics("zoom_while_moving", zoom_move);
    require_edge_safe("zoom_while_moving", zoom_move);

    const auto zoom_out = replay(zoom_out_near_edge_trace());
    print_metrics("zoom_out_near_edge", zoom_out);
    require_edge_safe("zoom_out_near_edge", zoom_out);
    require(std::fabs(zoom_out.final.zoom - 1.0f) < 0.01f,
            "zoom-out trace should return close to 1x without exposing an edge");
    require(arzoom::length(arzoom::sub(zoom_out.final.center, {0.5f, 0.5f})) < 0.01f,
            "zoom-out trace should settle close to canvas center");

    const DesktopRect negative_rect{-1920, -180, 0, 900};
    const auto normalized_origin = normalize_desktop_cursor(-1920, -180, negative_rect);
    const auto normalized_corner = normalize_desktop_cursor(-1, 899, negative_rect);
    require(arzoom::nearly_equal(normalized_origin, {0.0f, 0.0f}, 1.0e-6f),
            "negative-coordinate monitor origin should normalize to 0,0");
    require(normalized_corner.x > 0.999f && normalized_corner.y > 0.999f,
            "negative-coordinate monitor far corner should normalize close to 1,1");

    const auto negative_monitor = replay(negative_monitor_trace());
    print_metrics("negative_monitor", negative_monitor);
    require_edge_safe("negative_monitor", negative_monitor);

    const auto mixed_aspect = replay(mixed_aspect_trace());
    print_metrics("mixed_aspect", mixed_aspect);
    require_edge_safe("mixed_aspect", mixed_aspect);

    TraceMetrics fps_metrics[4];
    const int fps_values[4] = {30, 60, 120, 144};
    for (int i = 0; i < 4; ++i) {
        fps_metrics[i] = replay(long_relocation_trace(fps_values[i]));
        require_edge_safe("fps_consistency", fps_metrics[i]);
    }
    for (int i = 1; i < 4; ++i) {
        require(arzoom::length(arzoom::sub(fps_metrics[0].final.center, fps_metrics[i].final.center)) < 0.0015f,
                "30/60/120/144 fps traces should converge to equivalent framing");
        require(std::fabs(fps_metrics[0].final.zoom - fps_metrics[i].final.zoom) < 0.0015f,
                "30/60/120/144 fps traces should converge to equivalent zoom");
    }

    std::cout << "ArZoom Phase 0 deterministic trace suite: PASS\n";
    return 0;
}
