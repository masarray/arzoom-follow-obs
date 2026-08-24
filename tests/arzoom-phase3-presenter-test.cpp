#include "../src/arzoom-presenter-controls.hpp"

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

bool near(float a, float b, float epsilon = 1.0e-5f)
{
    return std::fabs(a - b) <= epsilon;
}

bool near(arzoom::Vec2 a, arzoom::Vec2 b, float epsilon = 1.0e-5f)
{
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

void hold_zoom_composes_with_latched_zoom()
{
    require(!arzoom::presenter_zoom_requested(false, false),
            "idle presenter state unexpectedly requests zoom");
    require(arzoom::presenter_zoom_requested(false, true),
            "Hold Zoom did not request zoom");
    require(arzoom::presenter_zoom_requested(true, false),
            "latched Toggle Zoom did not request zoom");
    require(arzoom::presenter_zoom_requested(true, true),
            "Hold Zoom masked an already-latched Toggle Zoom");
}

void zoom_steps_are_bounded_and_predictable()
{
    require(near(arzoom::presenter_zoom_step(2.0f, 0.25f), 2.25f),
            "Zoom In did not use the 0.25x presenter step");
    require(near(arzoom::presenter_zoom_step(2.0f, -0.25f), 1.75f),
            "Zoom Out did not use the 0.25x presenter step");
    require(near(arzoom::presenter_zoom_step(3.95f, 0.25f), 4.0f),
            "Zoom In exceeded the supported 4x ceiling");
    require(near(arzoom::presenter_zoom_step(1.15f, -0.25f), 1.10f),
            "Zoom Out exceeded the supported 1.10x floor");
}

arzoom::OverviewOutput run_until(
    arzoom::OverviewPeekController &controller,
    float fps, bool expect_active,
    int max_frames = 240)
{
    arzoom::OverviewOutput output;
    for (int frame = 0; frame < max_frames; ++frame) {
        output = controller.step(1.0f / fps, 0.34f, 0.32f);
        if (output.active == expect_active &&
            (!expect_active || output.phase == arzoom::OverviewPhase::Holding))
            return output;
    }
    return output;
}

void overview_peek_returns_exact_saved_transform()
{
    using namespace arzoom;
    const Vec2 saved_center{0.72f, 0.31f};
    constexpr float saved_zoom = 2.75f;

    OverviewPeekController controller;
    require(controller.begin(saved_center, saved_zoom),
            "Overview Peek did not start from a zoomed shot");

    OverviewOutput output = run_until(controller, 60.0f, true);
    require(output.phase == OverviewPhase::Holding,
            "Overview Peek did not reach its 1x hold state");
    require(near(output.zoom, 1.0f, 1.0e-6f) &&
                near(output.center, {0.5f, 0.5f}, 1.0e-6f),
            "Overview Peek did not lock exact full frame");

    controller.release(output.center, output.zoom);
    output = run_until(controller, 60.0f, false);
    require(output.restored,
            "Overview Peek did not report exact shot restoration");
    require(near(output.zoom, saved_zoom, 1.0e-6f),
            "Overview Peek restored the wrong zoom");
    require(near(output.center, saved_center, 2.0e-6f),
            "Overview Peek restored the wrong focus center");
}

void overview_trajectory_is_straight_in_screen_space()
{
    using namespace arzoom;
    const Vec2 saved_center{0.68f, 0.37f};
    constexpr float saved_zoom = 3.1f;
    const Vec2 probes[] = {{0.18f, 0.22f}, {0.72f, 0.34f}, {0.91f, 0.80f}};

    OverviewPeekController controller;
    require(controller.begin(saved_center, saved_zoom),
            "straight-path overview test could not start");

    const ScreenTransform start = screen_transform(saved_center, saved_zoom);
    const ScreenTransform finish = screen_transform({0.5f, 0.5f}, 1.0f);

    for (int frame = 0; frame < 30; ++frame) {
        const OverviewOutput output = controller.step(1.0f / 60.0f, 0.34f, 0.32f);
        const ScreenTransform current = screen_transform(output.center, output.zoom);

        for (Vec2 probe : probes) {
            const Vec2 a = add(mul(probe, start.scale), start.offset);
            const Vec2 b = add(mul(probe, finish.scale), finish.offset);
            const Vec2 p = add(mul(probe, current.scale), current.offset);
            const Vec2 ab = sub(b, a);
            const Vec2 ap = sub(p, a);
            const float cross = ab.x * ap.y - ab.y * ap.x;
            require(std::fabs(cross) < 2.5e-5f,
                    "Overview Peek bowed away from a straight screen-space path");
        }

        if (output.phase == OverviewPhase::Holding)
            break;
    }
}

void overview_release_ignores_cursor_motion_by_construction()
{
    using namespace arzoom;
    OverviewPeekController a;
    OverviewPeekController b;
    require(a.begin({0.70f, 0.29f}, 2.6f), "controller A failed to begin");
    require(b.begin({0.70f, 0.29f}, 2.6f), "controller B failed to begin");

    OverviewOutput oa;
    OverviewOutput ob;
    for (int frame = 0; frame < 24; ++frame) {
        oa = a.step(1.0f / 60.0f, 0.34f, 0.32f);
        ob = b.step(1.0f / 60.0f, 0.34f, 0.32f);
        /* Deliberately no cursor input exists in the controller API. This is a
         * regression contract: overview geometry cannot be retargeted by the
         * presenter moving the pointer while the key is held. */
        require(near(oa.center, ob.center, 1.0e-7f) &&
                    near(oa.zoom, ob.zoom, 1.0e-7f),
                "Overview Peek became dependent on external pointer activity");
    }
}

void reset_during_overview_finishes_at_exact_full_frame()
{
    using namespace arzoom;
    OverviewPeekController controller;
    require(controller.begin({0.76f, 0.42f}, 3.0f),
            "cancel test could not start Overview Peek");

    OverviewOutput output;
    for (int frame = 0; frame < 7; ++frame)
        output = controller.step(1.0f / 60.0f, 0.34f, 0.32f);

    controller.cancel_to_overview(output.center, output.zoom);
    output = run_until(controller, 60.0f, false);
    require(output.cancelled,
            "Reset path did not cancel Overview Peek restoration");
    require(near(output.zoom, 1.0f, 1.0e-6f) &&
                near(output.center, {0.5f, 0.5f}, 1.0e-6f),
            "Reset during Overview Peek did not end at exact full frame");
}

void overview_endpoints_are_frame_rate_consistent()
{
    using namespace arzoom;
    const float rates[] = {30.0f, 60.0f, 120.0f, 144.0f};
    const Vec2 saved_center{0.64f, 0.41f};
    constexpr float saved_zoom = 2.35f;

    for (float fps : rates) {
        OverviewPeekController controller;
        require(controller.begin(saved_center, saved_zoom),
                "frame-rate overview test failed to begin");
        OverviewOutput output = run_until(controller, fps, true, 400);
        require(output.phase == OverviewPhase::Holding,
                "overview did not reach hold state at tested frame rate");
        require(near(output.center, {0.5f, 0.5f}, 2.0e-6f) &&
                    near(output.zoom, 1.0f, 2.0e-6f),
                "overview full-frame endpoint changed with frame rate");

        controller.release(output.center, output.zoom);
        output = run_until(controller, fps, false, 400);
        require(output.restored,
                "overview return did not finish at tested frame rate");
        require(near(output.center, saved_center, 2.0e-6f) &&
                    near(output.zoom, saved_zoom, 2.0e-6f),
                "overview restored endpoint changed with frame rate");
    }
}

} // namespace

int main()
{
    hold_zoom_composes_with_latched_zoom();
    zoom_steps_are_bounded_and_predictable();
    overview_peek_returns_exact_saved_transform();
    overview_trajectory_is_straight_in_screen_space();
    overview_release_ignores_cursor_motion_by_construction();
    reset_during_overview_finishes_at_exact_full_frame();
    overview_endpoints_are_frame_rate_consistent();
    std::cout << "ArZoom Phase 3 presenter-control gates: PASS\n";
    return 0;
}
