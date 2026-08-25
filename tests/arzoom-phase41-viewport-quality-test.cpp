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

arzoom::CameraInput input(float dt, arzoom::Vec2 cursor,
                          bool zoom_requested, float zoom)
{
    arzoom::CameraInput value;
    value.dt = dt;
    value.cursor = cursor;
    value.cursor_valid = true;
    value.zoom_requested = zoom_requested;
    value.configured_zoom = zoom;
    value.anchor = {0.5f, 0.45f};
    value.safe_zone = 0.28f;
    value.follow_policy = arzoom::CameraFollowPolicy::Smart;
    value.motion_style = arzoom::CameraMotionStyle::Balanced;
    return value;
}

void warm(arzoom::PresenterAwareSmartCamera &camera,
          arzoom::Vec2 cursor, float zoom)
{
    camera.set_scene_context(true);
    camera.step(input(1.0f / 60.0f, cursor, false, zoom));
    for (int i = 0; i < 120; ++i)
        camera.step(input(1.0f / 60.0f, cursor, true, zoom));
}

void repeated_zoom_out_never_freezes()
{
    using namespace arzoom;
    PresenterAwareSmartCamera camera;
    const Vec2 cursor{0.84f, 0.72f};
    warm(camera, cursor, 3.0f);

    float target = 3.0f;
    float previous_zoom = camera.output().zoom;
    float max_frame_delta = 0.0f;

    for (int step = 0; step < 4; ++step) {
        target -= 0.25f;
        bool moved = false;
        for (int frame = 0; frame < 75; ++frame) {
            const CameraOutput out = camera.step(
                input(1.0f / 60.0f, cursor, true, target));
            max_frame_delta = std::max(
                max_frame_delta, std::fabs(out.zoom - previous_zoom));
            if (out.zoom < previous_zoom - 1.0e-5f)
                moved = true;
            require(out.zoom <= previous_zoom + 2.0e-4f,
                    "active Zoom Out reversed direction");
            previous_zoom = out.zoom;
        }
        require(moved, "active Zoom Out froze instead of moving");
        require(std::fabs(camera.output().zoom - target) < 0.012f,
                "active Zoom Out did not settle at requested level");
    }

    require(max_frame_delta < 0.045f,
            "active Zoom Out contained a visible one-frame zoom snap");
}

void active_zoom_step_is_pointer_anchored()
{
    using namespace arzoom;
    PresenterAwareSmartCamera camera;
    const Vec2 cursor{0.73f, 0.62f};
    warm(camera, cursor, 2.0f);

    const float target_zoom = 2.75f;
    float max_frame_delta = 0.0f;
    float previous_zoom = camera.output().zoom;
    Vec2 previous_center = camera.output().center;
    float max_center_step_output = 0.0f;

    for (int frame = 0; frame < 100; ++frame) {
        const CameraOutput out = camera.step(
            input(1.0f / 60.0f, cursor, true, target_zoom));
        max_frame_delta = std::max(
            max_frame_delta, std::fabs(out.zoom - previous_zoom));
        max_center_step_output = std::max(
            max_center_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_zoom = out.zoom;
        previous_center = out.center;
    }

    const CameraOutput settled = camera.output();
    const Vec2 pointer = cursor_output_position(
        cursor, settled.center, settled.zoom);
    require(std::fabs(settled.zoom - target_zoom) < 0.012f,
            "active Zoom In did not reach requested level");
    require(std::fabs(pointer.x - 0.5f) < 0.035f &&
                std::fabs(pointer.y - 0.45f) < 0.035f,
            "Zoom +/- did not frame around the pointer anchor");
    require(max_frame_delta < 0.060f,
            "active Zoom In contained a visible one-frame snap");
    require(max_center_step_output < 0.055f,
            "pointer-anchored zoom produced a visible pan snap");
}

void edge_pointer_has_visibility_priority()
{
    using namespace arzoom;
    PresenterAwareSmartCamera camera;
    warm(camera, {0.5f, 0.5f}, 2.5f);

    const Vec2 cursor{0.10f, 0.86f};
    bool entered_safe_output = false;
    int outside_frames = 0;
    float max_step_output = 0.0f;
    Vec2 previous_center = camera.output().center;

    for (int frame = 0; frame < 240; ++frame) {
        const CameraOutput out = camera.step(
            input(1.0f / 60.0f, cursor, true, 2.5f));
        const Vec2 pointer = cursor_output_position(
            cursor, out.center, out.zoom);
        if (pointer.x < 0.0f || pointer.x > 1.0f ||
            pointer.y < 0.0f || pointer.y > 1.0f) {
            ++outside_frames;
        }
        if (pointer.x >= 0.075f && pointer.x <= 0.925f &&
            pointer.y >= 0.075f && pointer.y <= 0.925f) {
            entered_safe_output = true;
        }

        max_step_output = std::max(
            max_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_center = out.center;
    }

    require(entered_safe_output,
            "edge pointer was never recovered into a visible viewport region");
    require(outside_frames < 18,
            "edge pointer stayed invisible for too many frames");
    require(max_step_output < 0.075f,
            "pointer visibility recovery snapped the viewport");
}

void coast_cannot_keep_pulling_away_from_pointer()
{
    using namespace arzoom;
    PresenterAwareSmartCamera camera;
    warm(camera, {0.5f, 0.5f}, 2.0f);

    const Vec2 cursor{0.82f, 0.70f};
    float previous_anchor_error = 10.0f;
    int sustained_worse = 0;
    int max_sustained_worse = 0;

    for (int frame = 0; frame < 360; ++frame) {
        const CameraOutput out = camera.step(
            input(1.0f / 60.0f, cursor, true, 2.0f));
        const Vec2 pointer = cursor_output_position(
            cursor, out.center, out.zoom);
        const float anchor_error = length(sub(pointer, Vec2{0.5f, 0.45f}));

        if (frame > 30 && anchor_error > previous_anchor_error + 0.0010f)
            ++sustained_worse;
        else
            sustained_worse = 0;
        max_sustained_worse = std::max(max_sustained_worse,
                                       sustained_worse);
        previous_anchor_error = anchor_error;
    }

    require(max_sustained_worse < 18,
            "camera kept drifting away from pointer context for too long");

    const CameraOutput settled = camera.output();
    const Vec2 final_pointer = cursor_output_position(
        cursor, settled.center, settled.zoom);
    require(final_pointer.x >= 0.07f && final_pointer.x <= 0.93f &&
                final_pointer.y >= 0.07f && final_pointer.y <= 0.93f,
            "settled Smart viewport left pointer outside useful view");
}

void high_zoom_final_pointer_context_is_recovered()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    PresenterAwareSmartCamera camera;
    Vec2 cursor{0.50f, 0.50f};
    float zoom = 2.0f;
    warm(camera, cursor, zoom);

    /* Reproduce the user interaction: press Increase repeatedly until the
     * viewport is small enough that stale framing becomes obvious. */
    for (int step = 0; step < 5; ++step) {
        zoom += 0.25f;
        for (int frame = 0; frame < 55; ++frame)
            camera.step(input(dt, cursor, true, zoom));
    }
    require(std::fabs(camera.output().zoom - zoom) < 0.012f,
            "five active Increase steps did not reach the high zoom target");

    const Vec2 start = cursor;
    const Vec2 final_cursor{0.32f, 0.76f};
    Vec2 previous_center = camera.output().center;
    float max_step_output = 0.0f;

    /* Move from centre toward the middle/lower-left region. The camera may
     * remain calm during motion; the contract is about the final context. */
    for (int frame = 1; frame <= 45; ++frame) {
        const float t = static_cast<float>(frame) / 45.0f;
        cursor = lerp(start, final_cursor, t);
        const CameraOutput out = camera.step(input(dt, cursor, true, zoom));
        max_step_output = std::max(
            max_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_center = out.center;
    }

    bool final_context_reached = false;
    int final_pointer_invisible_frames = 0;
    for (int frame = 0; frame < 150; ++frame) {
        const CameraOutput out = camera.step(
            input(dt, final_cursor, true, zoom));
        const Vec2 pointer = cursor_output_position(
            final_cursor, out.center, out.zoom);
        if (pointer.x < 0.0f || pointer.x > 1.0f ||
            pointer.y < 0.0f || pointer.y > 1.0f) {
            ++final_pointer_invisible_frames;
        }
        if (pointer.x >= 0.16f && pointer.x <= 0.84f &&
            pointer.y >= 0.16f && pointer.y <= 0.84f) {
            final_context_reached = true;
        }
        max_step_output = std::max(
            max_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_center = out.center;
    }

    const CameraOutput settled = camera.output();
    const Vec2 final_pointer = cursor_output_position(
        final_cursor, settled.center, settled.zoom);
    require(final_context_reached,
            "high zoom never reframed around the settled final pointer context");
    require(final_pointer_invisible_frames < 36,
            "high zoom left the settled final pointer invisible for too long");
    require(final_pointer.x >= 0.15f && final_pointer.x <= 0.85f &&
                final_pointer.y >= 0.15f && final_pointer.y <= 0.85f,
            "high zoom settled on stale viewport context instead of final mouse location");
    require(max_step_output < 0.075f,
            "high-zoom final-context recovery introduced a viewport snap");
}

void stale_corner_target_rebases_to_final_pointer()
{
    using namespace arzoom;
    constexpr float dt = 1.0f / 60.0f;
    PresenterAwareSmartCamera camera;
    warm(camera, {0.50f, 0.50f}, 3.5f);

    /* Start a recovery toward lower-left, then move the pointer to a different
     * final context while that shot is in flight. The old corner target must
     * not remain authoritative. */
    const Vec2 stale_corner{0.08f, 0.90f};
    for (int frame = 0; frame < 18; ++frame)
        camera.step(input(dt, stale_corner, true, 3.5f));

    const Vec2 final_cursor{0.40f, 0.67f};
    Vec2 previous_center = camera.output().center;
    float max_step_output = 0.0f;
    for (int frame = 0; frame < 150; ++frame) {
        const CameraOutput out = camera.step(
            input(dt, final_cursor, true, 3.5f));
        max_step_output = std::max(
            max_step_output,
            length(sub(out.center, previous_center)) * out.zoom);
        previous_center = out.center;
    }

    const CameraOutput settled = camera.output();
    const Vec2 final_pointer = cursor_output_position(
        final_cursor, settled.center, settled.zoom);
    require(final_pointer.x >= 0.15f && final_pointer.x <= 0.85f &&
                final_pointer.y >= 0.15f && final_pointer.y <= 0.85f,
            "viewport kept prioritizing obsolete lower-left target after pointer moved");
    require(max_step_output < 0.075f,
            "stale-target rebase introduced a viewport snap");
}

} // namespace

int main()
{
    repeated_zoom_out_never_freezes();
    active_zoom_step_is_pointer_anchored();
    edge_pointer_has_visibility_priority();
    coast_cannot_keep_pulling_away_from_pointer();
    high_zoom_final_pointer_context_is_recovered();
    stale_corner_target_rebases_to_final_pointer();
    std::cout << "ArZoom P4.1 viewport quality gates: PASS\n";
    return 0;
}
