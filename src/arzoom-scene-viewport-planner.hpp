#pragma once

#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

/*
 * Deterministic Scene Viewport Planner
 * ====================================
 *
 * Managed Scene Camera uses one exclusive decision loop:
 *
 *   OBSERVE pointer -> SETTLE -> PLAN ONCE -> COMMIT -> MINIMUM-JERK -> HOLD
 *
 * There are no parallel Coast/edge/visibility controllers and no hidden camera
 * synchronization. A committed target is immutable until either it completes,
 * the presenter explicitly changes zoom, or a materially different pointer has
 * itself settled and supersedes the old intent.
 *
 * The planner is intentionally tiny and O(1): one pointer sample, a few scalar
 * comparisons, closed-form viewport geometry, and one quintic trajectory.
 */

struct SceneViewportPlan {
    bool active = false;
    Vec2 target_center{0.5f, 0.5f};
    Vec2 target_pointer_output{0.5f, 0.5f};
    float travel_output = 0.0f;
};

inline float scene_pointer_settle_seconds(CameraMotionStyle style)
{
    switch (style) {
    case CameraMotionStyle::Responsive:
        return 0.09f;
    case CameraMotionStyle::Balanced:
        return 0.12f;
    case CameraMotionStyle::Cinematic:
    default:
        return 0.15f;
    }
}

inline float scene_context_wake_half(float safe_zone)
{
    /* Default safe_zone=0.28 -> ~0.115 output half-width. Around the default
     * anchor this wakes near the inner rule-of-thirds rather than waiting for
     * the pointer to reach the viewport centre. */
    return std::clamp(safe_zone * 0.41f, 0.105f, 0.165f);
}

inline float scene_context_landing_half(float wake_half)
{
    /* Land clearly inside the wake envelope so exact HOLD cannot immediately
     * request a second correction. */
    return std::clamp(wake_half * 0.66f, 0.065f, 0.105f);
}

inline SceneViewportPlan scene_context_plan(const CameraInput &input,
                                            Vec2 current_center,
                                            float zoom)
{
    SceneViewportPlan plan;
    if (!input.cursor_valid ||
        input.follow_policy == CameraFollowPolicy::Fixed)
        return plan;

    const float safe_zoom = std::max(zoom, 1.0f);
    const Vec2 pointer_output = cursor_output_position(
        input.cursor, current_center, safe_zoom);

    Vec2 desired_output = pointer_output;
    bool needs_move = false;

    if (input.follow_policy == CameraFollowPolicy::Centered) {
        const Vec2 anchor{
            std::clamp(input.anchor.x, 0.12f, 0.88f),
            std::clamp(input.anchor.y, 0.12f, 0.88f),
        };
        const float error = length(sub(pointer_output, anchor));
        if (error <= 0.030f)
            return plan;
        desired_output = anchor;
        needs_move = true;
    } else {
        const float wake_half = scene_context_wake_half(input.safe_zone);
        const float landing_half = scene_context_landing_half(wake_half);
        const Vec2 anchor{
            std::clamp(input.anchor.x, 0.20f, 0.80f),
            std::clamp(input.anchor.y, 0.20f, 0.80f),
        };

        const float wake_left = anchor.x - wake_half;
        const float wake_right = anchor.x + wake_half;
        const float wake_top = anchor.y - wake_half;
        const float wake_bottom = anchor.y + wake_half;

        if (pointer_output.x < wake_left) {
            desired_output.x = anchor.x - landing_half;
            needs_move = true;
        } else if (pointer_output.x > wake_right) {
            desired_output.x = anchor.x + landing_half;
            needs_move = true;
        }

        if (pointer_output.y < wake_top) {
            desired_output.y = anchor.y - landing_half;
            needs_move = true;
        } else if (pointer_output.y > wake_bottom) {
            desired_output.y = anchor.y + landing_half;
            needs_move = true;
        }
    }

    if (!needs_move)
        return plan;

    Vec2 target_center{
        current_center.x +
            (pointer_output.x - desired_output.x) / safe_zoom,
        current_center.y +
            (pointer_output.y - desired_output.y) / safe_zoom,
    };
    target_center = clamp_center(target_center, safe_zoom);

    const float travel_output =
        length(sub(target_center, current_center)) * safe_zoom;
    if (travel_output <= 0.0075f)
        return plan; // Requested framing is already the best reachable edge.

    plan.active = true;
    plan.target_center = target_center;
    plan.target_pointer_output = cursor_output_position(
        input.cursor, target_center, safe_zoom);
    plan.travel_output = travel_output;
    return plan;
}

enum class SceneShotReason {
    None,
    Activation,
    ZoomStep,
    PointerContext,
    Return,
};

class SceneViewportPlanner {
public:
    SceneViewportPlanner() { reset(); }

    void reset()
    {
        output_ = {};
        output_.center = {0.5f, 0.5f};
        output_.zoom = 1.0f;
        output_.state = CameraState::Rest;
        initialized_ = false;
        previous_zoom_requested_ = false;
        requested_zoom_target_ = 2.0f;
        pointer_tracker_valid_ = false;
        tracked_cursor_ = {0.5f, 0.5f};
        pointer_still_elapsed_ = 0.0f;
        pointer_motion_output_ = 0.0f;
        shot_active_ = false;
        shot_reason_ = SceneShotReason::None;
        shot_elapsed_ = 0.0f;
        shot_duration_ = 0.40f;
        shot_start_ = screen_transform({0.5f, 0.5f}, 1.0f);
        shot_target_ = shot_start_;
        committed_cursor_valid_ = false;
        committed_cursor_ = {0.5f, 0.5f};
        generation_ = 0;
    }

    CameraOutput output() const { return output_; }
    unsigned long long generation() const { return generation_; }
    bool shot_active() const { return shot_active_; }

    CameraOutput step(const CameraInput &source_input)
    {
        CameraInput input = source_input;
        const float dt = std::clamp(input.dt, 0.0f, 0.10f);
        const float desired_zoom =
            std::clamp(input.configured_zoom, 1.10f, 4.00f);
        const CameraProfile profile = camera_profile(input.motion_style);

        update_pointer_tracker(input, dt);

        if (!initialized_) {
            initialized_ = true;
            previous_zoom_requested_ = false;
            requested_zoom_target_ = desired_zoom;
        }

        const bool rising =
            input.zoom_requested && !previous_zoom_requested_;
        const bool falling =
            !input.zoom_requested && previous_zoom_requested_;
        const bool zoom_step =
            input.zoom_requested && previous_zoom_requested_ &&
            std::fabs(desired_zoom - requested_zoom_target_) > 0.0005f;

        if (falling) {
            commit_return(profile);
        } else if (rising) {
            commit_zoom_shot(input, desired_zoom, profile,
                             SceneShotReason::Activation);
        } else if (zoom_step) {
            commit_zoom_shot(input, desired_zoom, profile,
                             SceneShotReason::ZoomStep);
        }

        requested_zoom_target_ = desired_zoom;
        previous_zoom_requested_ = input.zoom_requested;

        if (!input.zoom_requested) {
            if (!shot_active_ &&
                (output_.zoom > 1.0005f ||
                 !nearly_equal(output_.center, {0.5f, 0.5f}, 0.0005f))) {
                commit_return(profile);
            }
            if (shot_active_)
                return step_committed_shot(dt);
            lock_full_frame();
            return output_;
        }

        if (shot_active_) {
            maybe_supersede_stale_pointer_target(input, profile);
            return step_committed_shot(dt);
        }

        if (input.follow_policy != CameraFollowPolicy::Fixed &&
            input.cursor_valid && pointer_has_settled(input.motion_style)) {
            const SceneViewportPlan plan = scene_context_plan(
                input, output_.center, output_.zoom);
            if (plan.active) {
                commit_context_shot(input, plan, profile);
                return step_committed_shot(dt);
            }
        }

        exact_hold();
        return output_;
    }

private:
    bool pointer_has_settled(CameraMotionStyle style) const
    {
        return pointer_tracker_valid_ &&
               pointer_still_elapsed_ >= scene_pointer_settle_seconds(style);
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

        const float zoom = std::max(output_.zoom, 1.0f);
        pointer_motion_output_ =
            length(sub(input.cursor, tracked_cursor_)) * zoom;

        /* GetCursorPos is integer based; at rest it is exactly stable. Reset on
         * about one output-pixel of intentional motion at common resolutions so
         * slow cursor travel is not misclassified as a final pointer. */
        if (pointer_motion_output_ <= 0.0012f)
            pointer_still_elapsed_ = std::min(pointer_still_elapsed_ + dt, 2.0f);
        else
            pointer_still_elapsed_ = 0.0f;

        tracked_cursor_ = input.cursor;
    }

    Vec2 zoom_target_center(const CameraInput &input, float target_zoom) const
    {
        if (input.follow_policy == CameraFollowPolicy::Fixed ||
            !input.cursor_valid)
            return clamp_center(output_.center, target_zoom);

        const Vec2 anchor{
            std::clamp(input.anchor.x, 0.18f, 0.82f),
            std::clamp(input.anchor.y, 0.18f, 0.82f),
        };
        return centered_target(input.cursor, anchor, target_zoom);
    }

    float context_duration(const CameraProfile &profile,
                           float travel_output) const
    {
        const float base = std::clamp(
            profile.camera_filter_seconds + 0.055f, 0.26f, 0.44f);
        return std::clamp(base + 0.12f * travel_output,
                          0.26f, 0.50f);
    }

    void commit_shot(Vec2 target_center, float target_zoom,
                     float duration, SceneShotReason reason,
                     const CameraInput *input)
    {
        shot_start_ = screen_transform(output_.center,
                                       std::max(output_.zoom, 1.0f));
        shot_target_ = screen_transform(
            clamp_center(target_center, target_zoom), target_zoom);
        shot_elapsed_ = 0.0f;
        shot_duration_ = std::max(duration, 0.08f);
        shot_reason_ = reason;
        shot_active_ = true;
        ++generation_;

        committed_cursor_valid_ = input && input->cursor_valid;
        committed_cursor_ = committed_cursor_valid_
                                ? input->cursor
                                : output_.center;
    }

    void commit_zoom_shot(const CameraInput &input, float target_zoom,
                          const CameraProfile &profile,
                          SceneShotReason reason)
    {
        const bool zooming_in = target_zoom >= output_.zoom;
        const float duration = zooming_in
            ? std::clamp(profile.zoom_in_seconds, 0.30f, 0.58f)
            : std::clamp(profile.zoom_out_seconds, 0.34f, 0.66f);
        commit_shot(zoom_target_center(input, target_zoom), target_zoom,
                    duration, reason, &input);
    }

    void commit_context_shot(const CameraInput &input,
                             const SceneViewportPlan &plan,
                             const CameraProfile &profile)
    {
        commit_shot(plan.target_center, output_.zoom,
                    context_duration(profile, plan.travel_output),
                    SceneShotReason::PointerContext, &input);
    }

    void commit_return(const CameraProfile &profile)
    {
        CameraInput no_pointer;
        no_pointer.cursor_valid = false;
        commit_shot({0.5f, 0.5f}, 1.0f,
                    std::clamp(profile.zoom_out_seconds, 0.34f, 0.70f),
                    SceneShotReason::Return, nullptr);
    }

    void maybe_supersede_stale_pointer_target(const CameraInput &input,
                                               const CameraProfile &profile)
    {
        if (!shot_active_ || shot_reason_ == SceneShotReason::Return ||
            !input.cursor_valid || !committed_cursor_valid_ ||
            !pointer_has_settled(input.motion_style))
            return;

        const float moved_output =
            length(sub(input.cursor, committed_cursor_)) *
            std::max(output_.zoom, 1.0f);
        if (moved_output <= 0.070f)
            return;

        /* A new settled pointer is a new presenter intent. Supersede the old
         * generation exactly once from the currently visible transform. There
         * is never more than one active target. */
        if (shot_reason_ == SceneShotReason::Activation ||
            shot_reason_ == SceneShotReason::ZoomStep) {
            commit_zoom_shot(input, requested_zoom_target_, profile,
                             SceneShotReason::ZoomStep);
            return;
        }

        const SceneViewportPlan plan = scene_context_plan(
            input, output_.center, output_.zoom);
        if (plan.active) {
            commit_context_shot(input, plan, profile);
        } else {
            /* Latest final pointer is already well framed in the current
             * viewport. Cancel the obsolete shot and HOLD exactly here. */
            shot_active_ = false;
            shot_reason_ = SceneShotReason::None;
            committed_cursor_ = input.cursor;
            committed_cursor_valid_ = true;
            exact_hold();
        }
    }

    CameraOutput step_committed_shot(float dt)
    {
        if (!shot_active_)
            return output_;

        const CameraOutput previous = output_;
        shot_elapsed_ += dt;
        const float normalized = std::clamp(
            shot_elapsed_ / std::max(shot_duration_, 0.05f), 0.0f, 1.0f);
        const ScreenTransform transform = lerp_transform(
            shot_start_, shot_target_, minimum_jerk01(normalized));

        output_.zoom = std::max(transform.scale, 1.0f);
        output_.center = clamp_center(transform_center(transform), output_.zoom);
        output_.intent_confidence = 1.0f;
        output_.urgency = 0.0f;
        switch (shot_reason_) {
        case SceneShotReason::Activation:
            output_.state = CameraState::Activating;
            break;
        case SceneShotReason::ZoomStep:
            output_.state = CameraState::Settle;
            break;
        case SceneShotReason::PointerContext:
            output_.state = CameraState::Follow;
            break;
        case SceneShotReason::Return:
            output_.state = CameraState::Returning;
            break;
        case SceneShotReason::None:
        default:
            output_.state = CameraState::SmoothIdle;
            break;
        }

        if (dt > 1.0e-6f) {
            const Vec2 next_velocity =
                mul(sub(output_.center, previous.center), 1.0f / dt);
            output_.acceleration =
                mul(sub(next_velocity, previous.velocity), 1.0f / dt);
            output_.velocity = next_velocity;
        } else {
            output_.velocity = {0.0f, 0.0f};
            output_.acceleration = {0.0f, 0.0f};
        }

        if (normalized >= 1.0f) {
            output_.zoom = std::max(shot_target_.scale, 1.0f);
            output_.center = clamp_center(
                transform_center(shot_target_), output_.zoom);
            output_.velocity = {0.0f, 0.0f};
            output_.acceleration = {0.0f, 0.0f};
            const bool returning = shot_reason_ == SceneShotReason::Return;
            shot_active_ = false;
            shot_reason_ = SceneShotReason::None;
            if (returning) {
                lock_full_frame();
            } else {
                output_.state = CameraState::SmoothIdle;
                output_.intent_confidence = 1.0f;
                output_.urgency = 0.0f;
            }
        }
        return output_;
    }

    void exact_hold()
    {
        output_.center = clamp_center(output_.center, output_.zoom);
        output_.velocity = {0.0f, 0.0f};
        output_.acceleration = {0.0f, 0.0f};
        output_.state = CameraState::SmoothIdle;
        output_.intent_confidence = 1.0f;
        output_.urgency = 0.0f;
    }

    void lock_full_frame()
    {
        output_.center = {0.5f, 0.5f};
        output_.zoom = 1.0f;
        output_.velocity = {0.0f, 0.0f};
        output_.acceleration = {0.0f, 0.0f};
        output_.state = CameraState::Rest;
        output_.intent_confidence = 0.0f;
        output_.urgency = 0.0f;
        shot_active_ = false;
        shot_reason_ = SceneShotReason::None;
    }

    CameraOutput output_{};
    bool initialized_ = false;
    bool previous_zoom_requested_ = false;
    float requested_zoom_target_ = 2.0f;

    bool pointer_tracker_valid_ = false;
    Vec2 tracked_cursor_{0.5f, 0.5f};
    float pointer_still_elapsed_ = 0.0f;
    float pointer_motion_output_ = 0.0f;

    bool shot_active_ = false;
    SceneShotReason shot_reason_ = SceneShotReason::None;
    float shot_elapsed_ = 0.0f;
    float shot_duration_ = 0.40f;
    ScreenTransform shot_start_{};
    ScreenTransform shot_target_{};
    bool committed_cursor_valid_ = false;
    Vec2 committed_cursor_{0.5f, 0.5f};
    unsigned long long generation_ = 0;
};

} // namespace arzoom
