#pragma once

#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

struct EdgeContextPlan {
    bool active = false;
    Vec2 target_center{0.5f, 0.5f};
    Vec2 target_pointer_output{0.5f, 0.5f};
};

/*
 * Directional edge release
 * ------------------------
 * A viewport that has physically clamped to a scene edge has an asymmetric
 * presentation problem: movement farther toward that edge is impossible, while
 * movement back toward the interior is meaningful much earlier than the legacy
 * symmetric SmoothIdle radius suggests. Treat roughly the inner rule-of-thirds
 * crossing as a wake signal, then move only enough to restore breathing room.
 *
 * The release is directional on all four edges and corners. Local pointer work
 * near the pinned edge remains idle; only settled motion back toward the scene
 * interior wakes the viewport. This is intentionally not continuous pointer
 * following.
 */
inline EdgeContextPlan edge_context_release_plan(Vec2 cursor, Vec2 center,
                                                 float zoom, bool settled)
{
    EdgeContextPlan plan;
    const float safe_zoom = std::max(zoom, 1.0f);
    if (!settled || safe_zoom <= 1.0005f)
        return plan;

    const float half = 0.5f / safe_zoom;
    const float edge_epsilon = 0.012f / safe_zoom;
    const bool pinned_left = center.x <= half + edge_epsilon;
    const bool pinned_right = center.x >= 1.0f - half - edge_epsilon;
    const bool pinned_top = center.y <= half + edge_epsilon;
    const bool pinned_bottom = center.y >= 1.0f - half - edge_epsilon;

    if (!pinned_left && !pinned_right && !pinned_top && !pinned_bottom)
        return plan;

    const Vec2 pointer_output = cursor_output_position(cursor, center, safe_zoom);
    Vec2 desired_output = pointer_output;
    bool release = false;

    constexpr float wake_low = 0.37f;
    constexpr float wake_high = 0.63f;
    constexpr float settle_low = 0.44f;
    constexpr float settle_high = 0.56f;

    if (pinned_left && pointer_output.x >= wake_high) {
        desired_output.x = settle_high;
        release = true;
    }
    if (pinned_right && pointer_output.x <= wake_low) {
        desired_output.x = settle_low;
        release = true;
    }
    if (pinned_top && pointer_output.y >= wake_high) {
        desired_output.y = settle_high;
        release = true;
    }
    if (pinned_bottom && pointer_output.y <= wake_low) {
        desired_output.y = settle_low;
        release = true;
    }

    if (!release)
        return plan;

    Vec2 target_center{
        center.x + (pointer_output.x - desired_output.x) / safe_zoom,
        center.y + (pointer_output.y - desired_output.y) / safe_zoom,
    };
    target_center = clamp_center(target_center, safe_zoom);

    const float travel_output =
        length(sub(target_center, center)) * safe_zoom;
    if (travel_output <= 0.008f)
        return plan;

    plan.active = true;
    plan.target_center = target_center;
    plan.target_pointer_output = cursor_output_position(
        cursor, target_center, safe_zoom);
    return plan;
}

/*
 * Presenter-aware Smart Camera coordinator
 * ----------------------------------------
 *
 * The accepted SmartCamera remains the one semantic follow engine. Scene Camera
 * P4.1 opts into presenter-specific coordination around that engine:
 *
 * 1. Active Zoom +/- is a joint affine viewport shot. Scale and center move
 *    together with one minimum-jerk trajectory toward the current pointer, so
 *    zoom-out never has to freeze merely to preserve an old off-centre frame.
 * 2. Pointer movement is not chased continuously. The coordinator measures
 *    pointer settling in output-space; when motion stops, the final pointer
 *    location becomes a context constraint and is recovered into a useful
 *    viewport region when necessary.
 * 3. A pointer that is genuinely leaving the viewport gets a minimum-
 *    displacement visibility shot. If the pointer moves materially while that
 *    shot is running, the shot is rebased from the current frame toward the new
 *    pointer instead of completing an obsolete corner target.
 * 4. A viewport physically pinned at top/bottom/left/right gets a directional
 *    rule-of-thirds wake zone. Pointer motion back toward the interior releases
 *    the edge earlier than legacy symmetric SmoothIdle hysteresis would.
 * 5. Near-edge pointer context has priority over stale COAST intent. The same
 *    SmartCamera keeps its proven gimbal semantics and receives an emphasis
 *    event only when old coast momentum is demonstrably counter-useful.
 *
 * Per-source ArZoom does not opt in and therefore delegates bit-for-bit to the
 * accepted P1 SmartCamera. There is no second semantic follow engine, scene
 * mutation, frame readback, or alternate scene render graph.
 */
class PresenterAwareSmartCamera {
public:
    PresenterAwareSmartCamera()
    {
        reset();
    }

    CameraOutput output() const
    {
        return public_output_;
    }

    void set_scene_context(bool enabled)
    {
        scene_context_enabled_ = enabled;
        if (!enabled) {
            zoom_shot_active_ = false;
            visibility_shot_active_ = false;
            return_shot_active_ = false;
            sync_active_ = false;
            zoom_lock_active_ = false;
            visibility_reframe_active_ = false;
            pointer_tracker_valid_ = false;
            pointer_still_elapsed_ = 0.0f;
            initialized_ = false;
            public_output_ = camera_.output();
        }
    }

    void reset()
    {
        camera_.reset();
        public_output_ = camera_.output();
        scene_context_enabled_ = false;
        initialized_ = false;
        previous_zoom_requested_ = false;
        requested_zoom_target_ = 2.0f;
        zoom_shot_active_ = false;
        visibility_shot_active_ = false;
        return_shot_active_ = false;
        sync_active_ = false;
        zoom_lock_active_ = false;
        visibility_reframe_active_ = false;
        shot_elapsed_ = 0.0f;
        shot_duration_ = 0.40f;
        sync_focus_valid_ = false;
        sync_focus_ = {0.5f, 0.5f};
        sync_anchor_ = {0.5f, 0.45f};
        sync_target_zoom_ = 2.0f;
        pointer_tracker_valid_ = false;
        tracked_cursor_ = {0.5f, 0.5f};
        pointer_still_elapsed_ = 0.0f;
        pointer_motion_output_ = 0.0f;
    }

    CameraOutput step(const CameraInput &source_input)
    {
        if (!scene_context_enabled_) {
            public_output_ = camera_.step(source_input);
            return public_output_;
        }

        CameraInput input = source_input;
        const float dt = std::clamp(input.dt, 0.0f, 0.10f);
        const float desired_zoom =
            std::clamp(input.configured_zoom, 1.10f, 4.00f);
        const CameraProfile profile = camera_profile(input.motion_style);

        update_pointer_tracker(input, dt);

        if (!initialized_) {
            initialized_ = true;
            requested_zoom_target_ = desired_zoom;
            previous_zoom_requested_ = input.zoom_requested;
        }

        const bool falling =
            !input.zoom_requested && previous_zoom_requested_;
        const bool active_zoom_step =
            input.zoom_requested && previous_zoom_requested_ &&
            std::fabs(desired_zoom - requested_zoom_target_) > 0.0005f;

        if (falling) {
            zoom_lock_active_ = false;
            if (zoom_shot_active_ || visibility_shot_active_ || sync_active_)
                begin_return_override(profile);
        } else if (active_zoom_step) {
            begin_pointer_zoom_shot(input, desired_zoom, profile);
        }

        requested_zoom_target_ = desired_zoom;
        previous_zoom_requested_ = input.zoom_requested;

        if (return_shot_active_)
            return step_return_override(input, dt);

        if (zoom_shot_active_)
            return step_pointer_zoom_shot(input, dt);

        if (visibility_shot_active_) {
            if (visibility_target_is_stale(input))
                begin_visibility_shot(input, profile);
            return step_visibility_shot(input, dt);
        }

        if (sync_active_) {
            retarget_sync_to_public_if_pointer_changed(input);
            synchronize_camera(input);
            if (!camera_close_to_public())
                return public_output_;
            sync_active_ = false;
        }

        if (should_begin_visibility_shot(input, public_output_)) {
            begin_visibility_shot(input, profile);
            return step_visibility_shot(input, dt);
        }

        CameraInput guarded = apply_context_guard(input, camera_.output());
        CameraOutput next = camera_.step(guarded);

        if (zoom_lock_active_ && input.zoom_requested &&
            std::fabs(desired_zoom - sync_target_zoom_) <= 0.0005f) {
            next.zoom = sync_target_zoom_;
            next.center = clamp_center(next.center, next.zoom);
        }

        public_output_ = next;
        return public_output_;
    }

private:
    static bool inside_output_margin(Vec2 output, float margin)
    {
        const float m = std::clamp(margin, 0.0f, 0.49f);
        return output.x >= m && output.x <= 1.0f - m &&
               output.y >= m && output.y <= 1.0f - m;
    }

    static bool inside_anchor_zone(Vec2 output, Vec2 anchor,
                                   float zone_width)
    {
        const float half =
            std::clamp(zone_width, 0.08f, 0.70f) * 0.5f;
        return std::fabs(output.x - anchor.x) <= half &&
               std::fabs(output.y - anchor.y) <= half;
    }

    static bool severely_outside_output(Vec2 output)
    {
        return output.x < -0.10f || output.x > 1.10f ||
               output.y < -0.10f || output.y > 1.10f;
    }

    static Vec2 presentation_anchor(Vec2 anchor)
    {
        return {
            std::clamp(anchor.x, 0.18f, 0.82f),
            std::clamp(anchor.y, 0.18f, 0.82f),
        };
    }

    static float settled_context_margin(float zoom)
    {
        return std::clamp(
            0.16f + 0.025f * (std::max(zoom, 1.0f) - 2.0f),
            0.16f, 0.22f);
    }

    static float pointer_settle_seconds(CameraMotionStyle style)
    {
        switch (style) {
        case CameraMotionStyle::Responsive:
            return 0.10f;
        case CameraMotionStyle::Balanced:
            return 0.13f;
        case CameraMotionStyle::Cinematic:
        default:
            return 0.16f;
        }
    }

    static float edge_release_settle_seconds(CameraMotionStyle style)
    {
        return std::max(0.075f, pointer_settle_seconds(style) * 0.72f);
    }

    static Vec2 minimum_visibility_center(Vec2 cursor, Vec2 current_center,
                                          float zoom, float margin)
    {
        const float safe_zoom = std::max(zoom, 1.0f);
        const float safe_margin = std::clamp(margin, 0.06f, 0.30f);
        const Vec2 output = cursor_output_position(
            cursor, current_center, safe_zoom);
        const Vec2 desired_output{
            std::clamp(output.x, safe_margin, 1.0f - safe_margin),
            std::clamp(output.y, safe_margin, 1.0f - safe_margin),
        };
        Vec2 target{
            current_center.x + (output.x - desired_output.x) / safe_zoom,
            current_center.y + (output.y - desired_output.y) / safe_zoom,
        };
        return clamp_center(target, safe_zoom);
    }

    EdgeContextPlan current_edge_release_plan(const CameraInput &input,
                                              const CameraOutput &before) const
    {
        const bool settled = pointer_still_elapsed_ >=
                             edge_release_settle_seconds(input.motion_style);
        return edge_context_release_plan(
            input.cursor, before.center, before.zoom, settled);
    }

    void update_pointer_tracker(const CameraInput &input, float dt)
    {
        if (!input.cursor_valid) {
            pointer_tracker_valid_ = false;
            pointer_still_elapsed_ = 0.0f;
            pointer_motion_output_ = 0.0f;
            return;
        }

        if (!pointer_tracker_valid_) {
            pointer_tracker_valid_ = true;
            tracked_cursor_ = input.cursor;
            pointer_still_elapsed_ = 0.0f;
            pointer_motion_output_ = 0.0f;
            return;
        }

        const float zoom = std::max(public_output_.zoom, 1.0f);
        pointer_motion_output_ =
            length(sub(input.cursor, tracked_cursor_)) * zoom;

        if (pointer_motion_output_ <= 0.0060f)
            pointer_still_elapsed_ = std::min(pointer_still_elapsed_ + dt, 2.0f);
        else
            pointer_still_elapsed_ = 0.0f;

        tracked_cursor_ = input.cursor;
    }

    void begin_pointer_zoom_shot(const CameraInput &input,
                                 float target_zoom,
                                 const CameraProfile &profile)
    {
        const float current_zoom = std::max(public_output_.zoom, 1.0f);
        const bool zooming_in = target_zoom > current_zoom;

        shot_start_ = screen_transform(public_output_.center, current_zoom);
        sync_focus_valid_ = input.cursor_valid &&
                            input.follow_policy != CameraFollowPolicy::Fixed;
        sync_focus_ = sync_focus_valid_ ? input.cursor : public_output_.center;
        sync_anchor_ = presentation_anchor(input.anchor);
        sync_target_zoom_ = target_zoom;

        Vec2 target_center = clamp_center(public_output_.center, target_zoom);
        if (input.follow_policy == CameraFollowPolicy::Fixed) {
            target_center = clamp_center({0.5f, 0.5f}, target_zoom);
        } else if (sync_focus_valid_) {
            target_center = centered_target(
                sync_focus_, sync_anchor_, target_zoom);
        }

        shot_target_ = screen_transform(target_center, target_zoom);
        shot_elapsed_ = 0.0f;
        shot_duration_ = zooming_in
            ? std::clamp(profile.zoom_in_seconds * 1.05f, 0.36f, 0.62f)
            : std::clamp(profile.zoom_out_seconds * 0.95f, 0.36f, 0.68f);
        zoom_shot_active_ = true;
        visibility_shot_active_ = false;
        return_shot_active_ = false;
        sync_active_ = true;
        zoom_lock_active_ = true;
        visibility_reframe_active_ = false;
    }

    bool should_begin_visibility_shot(const CameraInput &input,
                                      const CameraOutput &before) const
    {
        if (!input.zoom_requested || !input.cursor_valid ||
            input.follow_policy == CameraFollowPolicy::Fixed ||
            before.zoom <= 1.0005f ||
            before.state == CameraState::Activating ||
            before.state == CameraState::Returning) {
            return false;
        }

        const EdgeContextPlan edge_plan =
            current_edge_release_plan(input, before);
        if (edge_plan.active)
            return true;

        const Vec2 pointer_output = cursor_output_position(
            input.cursor, before.center, before.zoom);
        const float context_margin = settled_context_margin(before.zoom);
        const bool outside_visible =
            !inside_output_margin(pointer_output, 0.055f);
        const bool outside_context =
            !inside_output_margin(pointer_output, context_margin);
        const bool hard_loss = severely_outside_output(pointer_output);
        const bool settled_for_visibility = pointer_still_elapsed_ >= 0.050f;
        const bool settled_for_context =
            pointer_still_elapsed_ >= pointer_settle_seconds(input.motion_style);

        if (!hard_loss && !(outside_visible && settled_for_visibility) &&
            !(outside_context && settled_for_context)) {
            return false;
        }

        const float target_margin = outside_visible
            ? std::max(0.15f, context_margin - 0.025f)
            : context_margin;
        const Vec2 target_center = minimum_visibility_center(
            input.cursor, before.center, before.zoom, target_margin);
        const float required_output_travel =
            length(sub(target_center, before.center)) * before.zoom;

        return required_output_travel > 0.009f;
    }

    void begin_visibility_shot(const CameraInput &input,
                               const CameraProfile &profile)
    {
        const float zoom = std::max(public_output_.zoom, 1.0f);
        const EdgeContextPlan edge_plan =
            current_edge_release_plan(input, public_output_);

        Vec2 target_center = public_output_.center;
        Vec2 target_pointer_output = cursor_output_position(
            input.cursor, public_output_.center, zoom);

        if (edge_plan.active) {
            target_center = edge_plan.target_center;
            target_pointer_output = edge_plan.target_pointer_output;
        } else {
            const Vec2 pointer_output = target_pointer_output;
            const bool outside_visible =
                !inside_output_margin(pointer_output, 0.055f);
            const float context_margin = settled_context_margin(zoom);
            const float target_margin = outside_visible
                ? std::max(0.15f, context_margin - 0.025f)
                : context_margin;
            target_center = minimum_visibility_center(
                input.cursor, public_output_.center, zoom, target_margin);
            target_pointer_output = cursor_output_position(
                input.cursor, target_center, zoom);
        }

        const float travel_output =
            length(sub(target_center, public_output_.center)) * zoom;

        shot_start_ = screen_transform(public_output_.center, zoom);
        shot_target_ = screen_transform(target_center, zoom);
        shot_elapsed_ = 0.0f;
        shot_duration_ = std::clamp(
            0.30f + 0.14f * travel_output,
            std::max(0.32f, profile.camera_filter_seconds * 0.90f), 0.46f);

        sync_focus_valid_ = true;
        sync_focus_ = input.cursor;
        sync_anchor_ = target_pointer_output;
        sync_target_zoom_ = zoom;

        visibility_shot_active_ = true;
        zoom_shot_active_ = false;
        return_shot_active_ = false;
        sync_active_ = true;
        visibility_reframe_active_ = true;
    }

    bool visibility_target_is_stale(const CameraInput &input) const
    {
        if (!input.cursor_valid || !sync_focus_valid_)
            return false;
        const float zoom = std::max(public_output_.zoom, 1.0f);
        const float moved_output =
            length(sub(input.cursor, sync_focus_)) * zoom;
        return moved_output > 0.055f;
    }

    void retarget_sync_to_public_if_pointer_changed(const CameraInput &input)
    {
        if (!input.cursor_valid || !sync_focus_valid_)
            return;

        const float zoom = std::max(public_output_.zoom, 1.0f);
        const float moved_output =
            length(sub(input.cursor, sync_focus_)) * zoom;
        if (moved_output <= 0.050f)
            return;

        sync_focus_ = input.cursor;
        sync_target_zoom_ = zoom;
        sync_anchor_ = cursor_output_position(
            input.cursor, public_output_.center, zoom);
    }

    CameraInput make_sync_input(const CameraInput &input) const
    {
        CameraInput sync = input;
        sync.zoom_requested = true;
        sync.configured_zoom = sync_target_zoom_;
        sync.emphasis_event = true;
        sync.motion_style = CameraMotionStyle::Responsive;

        const CameraOutput internal = camera_.output();
        const float required_zoom = minimum_zoom_for_center(internal.center);
        const bool zoom_out_blocked =
            sync_target_zoom_ + 0.012f < internal.zoom &&
            required_zoom > sync_target_zoom_ + 0.010f;

        if (zoom_out_blocked) {
            sync.follow_policy = CameraFollowPolicy::Fixed;
            sync.cursor_valid = false;
        } else if (sync_focus_valid_) {
            sync.follow_policy = CameraFollowPolicy::Centered;
            sync.cursor = sync_focus_;
            sync.cursor_valid = true;
            sync.anchor = sync_anchor_;
        }
        return sync;
    }

    void synchronize_camera(const CameraInput &input)
    {
        camera_.step(make_sync_input(input));
    }

    bool camera_close_to_public() const
    {
        const CameraOutput internal = camera_.output();
        const float zoom_error =
            std::fabs(internal.zoom - public_output_.zoom);
        const float center_error_output =
            length(sub(internal.center, public_output_.center)) *
            std::max(public_output_.zoom, 1.0f);
        return zoom_error <= 0.0008f && center_error_output <= 0.006f;
    }

    void update_public_motion(CameraOutput previous, float dt)
    {
        if (dt > 1.0e-6f) {
            const Vec2 next_velocity =
                mul(sub(public_output_.center, previous.center), 1.0f / dt);
            public_output_.acceleration =
                mul(sub(next_velocity, previous.velocity), 1.0f / dt);
            public_output_.velocity = next_velocity;
        } else {
            public_output_.velocity = {0.0f, 0.0f};
            public_output_.acceleration = {0.0f, 0.0f};
        }
    }

    CameraOutput step_pointer_zoom_shot(const CameraInput &input, float dt)
    {
        synchronize_camera(input);

        const CameraOutput previous = public_output_;
        shot_elapsed_ += dt;
        const float normalized = std::clamp(
            shot_elapsed_ / std::max(shot_duration_, 0.05f), 0.0f, 1.0f);
        const float progress = minimum_jerk01(normalized);
        const ScreenTransform transform =
            lerp_transform(shot_start_, shot_target_, progress);

        public_output_.zoom = std::max(transform.scale, 1.0f);
        public_output_.center = clamp_center(
            transform_center(transform), public_output_.zoom);
        public_output_.state = CameraState::Settle;
        public_output_.intent_confidence = 1.0f;
        public_output_.urgency = 0.0f;
        update_public_motion(previous, dt);

        if (normalized >= 1.0f) {
            public_output_.zoom = shot_target_.scale;
            public_output_.center = clamp_center(
                transform_center(shot_target_), public_output_.zoom);
            public_output_.velocity = {0.0f, 0.0f};
            public_output_.acceleration = {0.0f, 0.0f};
            zoom_shot_active_ = false;
        }
        return public_output_;
    }

    CameraOutput step_visibility_shot(const CameraInput &input, float dt)
    {
        synchronize_camera(input);

        const CameraOutput previous = public_output_;
        shot_elapsed_ += dt;
        const float normalized = std::clamp(
            shot_elapsed_ / std::max(shot_duration_, 0.05f), 0.0f, 1.0f);
        const ScreenTransform transform = lerp_transform(
            shot_start_, shot_target_, minimum_jerk01(normalized));

        public_output_.zoom = shot_target_.scale;
        public_output_.center = clamp_center(
            transform_center(transform), public_output_.zoom);
        public_output_.state = CameraState::CatchUp;
        public_output_.intent_confidence = 1.0f;
        public_output_.urgency = 1.0f;
        update_public_motion(previous, dt);

        if (normalized >= 1.0f) {
            public_output_.center = clamp_center(
                transform_center(shot_target_), public_output_.zoom);
            public_output_.velocity = {0.0f, 0.0f};
            public_output_.acceleration = {0.0f, 0.0f};
            visibility_shot_active_ = false;
            visibility_reframe_active_ = false;
        }
        return public_output_;
    }

    void begin_return_override(const CameraProfile &profile)
    {
        shot_start_ = screen_transform(
            public_output_.center, std::max(public_output_.zoom, 1.0f));
        shot_target_ = screen_transform({0.5f, 0.5f}, 1.0f);
        shot_elapsed_ = 0.0f;
        shot_duration_ = std::max(profile.zoom_out_seconds, 0.10f);
        zoom_shot_active_ = false;
        visibility_shot_active_ = false;
        return_shot_active_ = true;
        sync_active_ = false;
        zoom_lock_active_ = false;
        visibility_reframe_active_ = false;
    }

    CameraOutput step_return_override(const CameraInput &input, float dt)
    {
        CameraInput background = input;
        background.zoom_requested = false;
        camera_.step(background);

        const CameraOutput previous = public_output_;
        shot_elapsed_ += dt;
        const float normalized = std::clamp(
            shot_elapsed_ / std::max(shot_duration_, 0.05f), 0.0f, 1.0f);
        const ScreenTransform transform = lerp_transform(
            shot_start_, shot_target_, minimum_jerk01(normalized));
        public_output_.zoom = std::max(transform.scale, 1.0f);
        public_output_.center = clamp_center(
            transform_center(transform), public_output_.zoom);
        public_output_.state = CameraState::Returning;
        update_public_motion(previous, dt);

        if (normalized >= 1.0f) {
            camera_.reset();
            public_output_ = camera_.output();
            return_shot_active_ = false;
            pointer_tracker_valid_ = false;
            pointer_still_elapsed_ = 0.0f;
        }
        return public_output_;
    }

    CameraInput apply_context_guard(const CameraInput &input,
                                    const CameraOutput &before)
    {
        CameraInput guarded = input;
        if (!input.zoom_requested || !input.cursor_valid ||
            input.follow_policy == CameraFollowPolicy::Fixed) {
            visibility_reframe_active_ = false;
            return guarded;
        }

        if (before.state == CameraState::Activating ||
            before.state == CameraState::Returning) {
            visibility_reframe_active_ = false;
            return guarded;
        }

        const Vec2 pointer_output = cursor_output_position(
            input.cursor, before.center, before.zoom);
        const Vec2 preferred_center = centered_target(
            input.cursor, presentation_anchor(input.anchor), before.zoom);
        const Vec2 toward_preferred = sub(preferred_center, before.center);
        const bool coast_pulling_away =
            before.state == CameraState::Coast &&
            !inside_anchor_zone(pointer_output, input.anchor,
                                std::max(input.safe_zone, 0.14f)) &&
            length(toward_preferred) * before.zoom > 0.018f &&
            dot(before.velocity, toward_preferred) < -0.00015f;

        if (coast_pulling_away)
            guarded.emphasis_event = true;
        return guarded;
    }

    SmartCamera camera_;
    CameraOutput public_output_{};

    bool scene_context_enabled_ = false;
    bool initialized_ = false;
    bool previous_zoom_requested_ = false;
    float requested_zoom_target_ = 2.0f;

    bool zoom_shot_active_ = false;
    bool visibility_shot_active_ = false;
    bool return_shot_active_ = false;
    bool sync_active_ = false;
    bool zoom_lock_active_ = false;
    bool visibility_reframe_active_ = false;

    float shot_elapsed_ = 0.0f;
    float shot_duration_ = 0.40f;
    ScreenTransform shot_start_{};
    ScreenTransform shot_target_{};

    bool sync_focus_valid_ = false;
    Vec2 sync_focus_{0.5f, 0.5f};
    Vec2 sync_anchor_{0.5f, 0.45f};
    float sync_target_zoom_ = 2.0f;

    bool pointer_tracker_valid_ = false;
    Vec2 tracked_cursor_{0.5f, 0.5f};
    float pointer_still_elapsed_ = 0.0f;
    float pointer_motion_output_ = 0.0f;
};

} // namespace arzoom

/* arzoom-filter-v2.cpp historically names its field `arzoom::SmartCamera`.
 * Keep the source ABI/layout change localized: after this header is included,
 * that token resolves to the coordinator above. The legacy SmartCamera remains
 * the single semantic engine owned inside PresenterAwareSmartCamera. */
#define SmartCamera PresenterAwareSmartCamera
