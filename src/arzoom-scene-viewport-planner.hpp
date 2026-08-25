#pragma once

#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

/*
 * Deterministic Scene Viewport Planner
 * ====================================
 *
 * Managed Scene Camera has exactly one authority. The normal contract remains:
 *
 *   OBSERVE -> SETTLE -> PLAN ONCE -> COMMIT -> MINIMUM-JERK -> EXACT HOLD
 *
 * Trial 7 adds distance-adaptive tracking inside that same planner. Small/local
 * pointer work remains calm. A materially far-moving pointer gets a bounded,
 * predictive O(1) tracking phase before it can leave the useful frame. Once the
 * pointer settles, tracking stops and one final closed-form target is committed.
 *
 * Two envelopes drive the behavior:
 * - optimal context envelope: pointer should finish comfortably near the anchor;
 * - hard visibility envelope: a moving pointer approaching the frame boundary
 *   raises urgency immediately.
 *
 * There is no second camera engine, no frame history, no image analysis, no
 * hidden synchronization and no scene mutation. State and work stay O(1).
 */

struct SceneViewportPlan {
    bool active = false;
    Vec2 target_center{0.5f, 0.5f};
    Vec2 target_pointer_output{0.5f, 0.5f};
    float travel_output = 0.0f;
};

inline float scene_zoom_pressure(float zoom)
{
    return std::clamp((std::max(zoom, 1.0f) - 2.60f) / 1.40f,
                      0.0f, 1.0f);
}

inline float scene_pointer_settle_seconds(CameraMotionStyle style, float zoom)
{
    float base = 0.15f;
    switch (style) {
    case CameraMotionStyle::Responsive:
        base = 0.09f;
        break;
    case CameraMotionStyle::Balanced:
        base = 0.12f;
        break;
    case CameraMotionStyle::Cinematic:
    default:
        base = 0.15f;
        break;
    }

    const float pressure = scene_zoom_pressure(zoom);
    return std::max(0.055f, base * (1.0f - 0.45f * pressure));
}

inline float scene_context_wake_half(float safe_zone, float zoom)
{
    const float base = std::clamp(safe_zone * 0.41f, 0.105f, 0.165f);
    const float pressure = scene_zoom_pressure(zoom);
    return std::clamp(base * (1.0f - 0.28f * pressure),
                      0.078f, 0.165f);
}

inline float scene_context_landing_half(float wake_half, float zoom)
{
    const float pressure = scene_zoom_pressure(zoom);
    const float ratio = 0.66f - 0.16f * pressure;
    return std::clamp(wake_half * ratio, 0.040f, 0.105f);
}

/* Kept as a compatibility/profile helper for Trial-6 regression tests. */
inline float scene_high_zoom_guard_margin(float zoom)
{
    const float pressure = scene_zoom_pressure(zoom);
    return 0.055f + 0.145f * pressure;
}

inline bool scene_high_zoom_guard_needed(const CameraInput &input,
                                         Vec2 current_center,
                                         float zoom)
{
    if (!input.cursor_valid ||
        input.follow_policy == CameraFollowPolicy::Fixed ||
        scene_zoom_pressure(zoom) < 0.18f)
        return false;

    const Vec2 pointer_output = cursor_output_position(
        input.cursor, current_center, std::max(zoom, 1.0f));
    const float margin = scene_high_zoom_guard_margin(zoom);
    return pointer_output.x < margin || pointer_output.x > 1.0f - margin ||
           pointer_output.y < margin || pointer_output.y > 1.0f - margin;
}

inline float scene_high_zoom_retarget_threshold(float zoom)
{
    const float pressure = scene_zoom_pressure(zoom);
    return 0.070f - 0.025f * pressure;
}

inline float scene_guard_follow_time(float zoom)
{
    const float pressure = scene_zoom_pressure(zoom);
    return 0.19f - 0.065f * pressure;
}

inline float scene_guard_follow_max_output_speed(float zoom)
{
    const float pressure = scene_zoom_pressure(zoom);
    return 2.45f + 1.35f * pressure;
}

inline Vec2 scene_context_anchor(const CameraInput &input)
{
    return {
        std::clamp(input.anchor.x, 0.20f, 0.80f),
        std::clamp(input.anchor.y, 0.20f, 0.80f),
    };
}

inline float scene_hard_visibility_margin(float zoom)
{
    /* At normal zoom the outer ~6% is emergency territory. At 4x the viewport
     * is tiny, so we protect roughly the outer 14% before the pointer can vanish. */
    return 0.060f + 0.080f * scene_zoom_pressure(zoom);
}

inline float scene_axis_context_pressure(float value, float anchor,
                                         float wake_half)
{
    const float excess = std::max(0.0f, std::fabs(value - anchor) - wake_half);
    const float available = std::max(0.5f - wake_half, 0.05f);
    return std::clamp(excess / available, 0.0f, 1.0f);
}

inline float scene_visibility_pressure(Vec2 pointer_output, float zoom)
{
    const float margin = scene_hard_visibility_margin(zoom);
    const float low = margin;
    const float high = 1.0f - margin;

    float outside = 0.0f;
    if (pointer_output.x < low)
        outside = std::max(outside, (low - pointer_output.x) / std::max(low, 0.01f));
    else if (pointer_output.x > high)
        outside = std::max(outside, (pointer_output.x - high) / std::max(margin, 0.01f));

    if (pointer_output.y < low)
        outside = std::max(outside, (low - pointer_output.y) / std::max(low, 0.01f));
    else if (pointer_output.y > high)
        outside = std::max(outside, (pointer_output.y - high) / std::max(margin, 0.01f));

    if (pointer_output.x < 0.0f || pointer_output.x > 1.0f ||
        pointer_output.y < 0.0f || pointer_output.y > 1.0f)
        return 1.0f;

    return std::clamp(0.70f + 0.30f * outside, 0.0f, 1.0f);
}

inline float scene_follow_pressure(const CameraInput &input,
                                   Vec2 current_center, float zoom)
{
    if (!input.cursor_valid || input.follow_policy == CameraFollowPolicy::Fixed)
        return 0.0f;

    const float safe_zoom = std::max(zoom, 1.0f);
    const Vec2 pointer_output = cursor_output_position(
        input.cursor, current_center, safe_zoom);
    const Vec2 anchor = scene_context_anchor(input);
    const float wake_half = scene_context_wake_half(input.safe_zone, safe_zoom);

    const float context = std::max(
        scene_axis_context_pressure(pointer_output.x, anchor.x, wake_half),
        scene_axis_context_pressure(pointer_output.y, anchor.y, wake_half));

    const float margin = scene_hard_visibility_margin(safe_zoom);
    const bool near_hard_edge =
        pointer_output.x < margin || pointer_output.x > 1.0f - margin ||
        pointer_output.y < margin || pointer_output.y > 1.0f - margin;
    const float visibility = near_hard_edge
        ? scene_visibility_pressure(pointer_output, safe_zoom)
        : 0.0f;

    return std::clamp(std::max(context, visibility), 0.0f, 1.0f);
}

inline bool scene_adaptive_guard_needed(const CameraInput &input,
                                        Vec2 current_center, float zoom,
                                        float pointer_motion_output)
{
    if (!input.cursor_valid ||
        input.follow_policy == CameraFollowPolicy::Fixed ||
        pointer_motion_output <= 0.0012f)
        return false;

    const float pressure = scene_follow_pressure(input, current_center, zoom);
    const float threshold = 0.40f - 0.16f * scene_zoom_pressure(zoom);
    return pressure >= threshold;
}

inline float scene_adaptive_follow_time(float zoom, float pressure)
{
    const float z = scene_zoom_pressure(zoom);
    return std::clamp(0.205f - 0.080f * pressure - 0.050f * z,
                      0.075f, 0.205f);
}

inline float scene_adaptive_follow_max_output_speed(float zoom, float pressure)
{
    const float z = scene_zoom_pressure(zoom);
    return 2.70f + 2.00f * pressure + 1.10f * z;
}

inline float scene_pointer_lead_seconds(float zoom, float pressure)
{
    const float z = scene_zoom_pressure(zoom);
    return std::clamp(0.025f + 0.040f * pressure + 0.025f * z,
                      0.025f, 0.090f);
}

inline float scene_tracking_landing_half(float zoom, float pressure)
{
    const float z = scene_zoom_pressure(zoom);
    const float base = 0.135f - 0.055f * z;
    return std::clamp(base * (1.0f - 0.28f * pressure),
                      0.048f, 0.135f);
}

inline SceneViewportPlan scene_tracking_plan(const CameraInput &input,
                                             Vec2 tracking_cursor,
                                             Vec2 current_center,
                                             float zoom,
                                             float pressure)
{
    SceneViewportPlan plan;
    if (!input.cursor_valid || input.follow_policy == CameraFollowPolicy::Fixed)
        return plan;

    const float safe_zoom = std::max(zoom, 1.0f);
    const Vec2 pointer_output = cursor_output_position(
        tracking_cursor, current_center, safe_zoom);
    const Vec2 anchor = scene_context_anchor(input);
    const float landing = scene_tracking_landing_half(safe_zoom, pressure);

    Vec2 desired_output = pointer_output;
    bool needs_move = false;

    if (pointer_output.x < anchor.x - landing) {
        desired_output.x = anchor.x - landing;
        needs_move = true;
    } else if (pointer_output.x > anchor.x + landing) {
        desired_output.x = anchor.x + landing;
        needs_move = true;
    }

    if (pointer_output.y < anchor.y - landing) {
        desired_output.y = anchor.y - landing;
        needs_move = true;
    } else if (pointer_output.y > anchor.y + landing) {
        desired_output.y = anchor.y + landing;
        needs_move = true;
    }

    if (!needs_move)
        return plan;

    Vec2 target_center{
        current_center.x + (pointer_output.x - desired_output.x) / safe_zoom,
        current_center.y + (pointer_output.y - desired_output.y) / safe_zoom,
    };
    target_center = clamp_center(target_center, safe_zoom);

    const float travel_output =
        length(sub(target_center, current_center)) * safe_zoom;
    if (travel_output <= 0.0025f)
        return plan;

    plan.active = true;
    plan.target_center = target_center;
    plan.target_pointer_output = cursor_output_position(
        tracking_cursor, target_center, safe_zoom);
    plan.travel_output = travel_output;
    return plan;
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
        const float wake_half = scene_context_wake_half(
            input.safe_zone, safe_zoom);
        const float landing_half = scene_context_landing_half(
            wake_half, safe_zoom);
        const Vec2 anchor = scene_context_anchor(input);

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
        return plan;

    plan.active = true;
    plan.target_center = target_center;
    plan.target_pointer_output = cursor_output_position(
        input.cursor, target_center, safe_zoom);
    plan.travel_output = travel_output;
    return plan;
}

inline float scene_context_shot_seconds(const CameraProfile &profile,
                                        float zoom, float travel_output)
{
    const float base = std::clamp(
        profile.camera_filter_seconds + 0.055f, 0.26f, 0.44f);
    const float z = scene_zoom_pressure(zoom);
    const float distance = std::clamp((travel_output - 0.035f) / 0.42f,
                                      0.0f, 1.0f);
    /* Far movement should be more decisive, not slower. */
    return std::clamp(base * (1.0f - 0.24f * z - 0.34f * distance),
                      0.18f, 0.44f);
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
        pointer_velocity_scene_ = {0.0f, 0.0f};
        shot_active_ = false;
        shot_reason_ = SceneShotReason::None;
        shot_elapsed_ = 0.0f;
        shot_duration_ = 0.40f;
        shot_start_ = screen_transform({0.5f, 0.5f}, 1.0f);
        shot_target_ = shot_start_;
        committed_cursor_valid_ = false;
        committed_cursor_ = {0.5f, 0.5f};
        guard_follow_active_ = false;
        generation_ = 0;
    }

    CameraOutput output() const { return output_; }
    unsigned long long generation() const { return generation_; }
    bool shot_active() const { return shot_active_; }
    bool guard_follow_active() const { return guard_follow_active_; }

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
            guard_follow_active_ = false;
            commit_return(profile);
        } else if (rising) {
            guard_follow_active_ = false;
            commit_zoom_shot(input, desired_zoom, profile,
                             SceneShotReason::Activation);
        } else if (zoom_step) {
            guard_follow_active_ = false;
            commit_zoom_shot(input, desired_zoom, profile,
                             SceneShotReason::ZoomStep);
        }

        requested_zoom_target_ = desired_zoom;
        previous_zoom_requested_ = input.zoom_requested;

        if (!input.zoom_requested) {
            guard_follow_active_ = false;
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

        const bool settled =
            input.follow_policy != CameraFollowPolicy::Fixed &&
            input.cursor_valid && pointer_has_settled(input.motion_style);
        const bool adaptive_guard = scene_adaptive_guard_needed(
            input, output_.center, output_.zoom, pointer_motion_output_);

        if (shot_active_) {
            /* Context shots may yield to a materially far moving pointer.
             * Explicit zoom trajectories stay atomic so scale remains clean. */
            if (shot_reason_ == SceneShotReason::PointerContext &&
                adaptive_guard && !settled) {
                shot_active_ = false;
                shot_reason_ = SceneShotReason::None;
                committed_cursor_valid_ = false;
                guard_follow_active_ = true;
                return step_adaptive_guard_follow(input, dt);
            }

            maybe_supersede_stale_pointer_target(input, profile);
            return step_committed_shot(dt);
        }

        if (adaptive_guard && !settled) {
            guard_follow_active_ = true;
            return step_adaptive_guard_follow(input, dt);
        }

        guard_follow_active_ = false;
        if (settled) {
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
               pointer_still_elapsed_ >=
                   scene_pointer_settle_seconds(style, output_.zoom);
    }

    void update_pointer_tracker(const CameraInput &input, float dt)
    {
        if (!input.cursor_valid) {
            pointer_tracker_valid_ = false;
            pointer_still_elapsed_ = 0.0f;
            pointer_motion_output_ = 0.0f;
            pointer_velocity_scene_ = {0.0f, 0.0f};
            return;
        }

        if (!pointer_tracker_valid_) {
            pointer_tracker_valid_ = true;
            tracked_cursor_ = input.cursor;
            pointer_still_elapsed_ = 0.0f;
            pointer_motion_output_ = 0.0f;
            pointer_velocity_scene_ = {0.0f, 0.0f};
            return;
        }

        const float zoom = std::max(output_.zoom, 1.0f);
        const Vec2 delta = sub(input.cursor, tracked_cursor_);
        pointer_motion_output_ = length(delta) * zoom;

        if (dt > 1.0e-6f) {
            const Vec2 instant_velocity = mul(delta, 1.0f / dt);
            const float alpha = exponential_alpha(dt, 0.080f);
            pointer_velocity_scene_ = add(
                mul(pointer_velocity_scene_, 1.0f - alpha),
                mul(instant_velocity, alpha));
        }

        if (pointer_motion_output_ <= 0.0012f) {
            pointer_still_elapsed_ = std::min(pointer_still_elapsed_ + dt, 2.0f);
            if (pointer_still_elapsed_ > 0.10f)
                pointer_velocity_scene_ = mul(pointer_velocity_scene_, 0.65f);
        } else {
            pointer_still_elapsed_ = 0.0f;
        }

        tracked_cursor_ = input.cursor;
    }

    Vec2 predicted_tracking_cursor(const CameraInput &input,
                                   float pressure) const
    {
        const float lead = scene_pointer_lead_seconds(output_.zoom, pressure);
        Vec2 predicted = add(input.cursor, mul(pointer_velocity_scene_, lead));
        predicted.x = std::clamp(predicted.x, 0.0f, 1.0f);
        predicted.y = std::clamp(predicted.y, 0.0f, 1.0f);
        return predicted;
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

    void commit_shot(Vec2 target_center, float target_zoom,
                     float duration, SceneShotReason reason,
                     const CameraInput *input)
    {
        guard_follow_active_ = false;
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
                    scene_context_shot_seconds(profile, output_.zoom,
                                               plan.travel_output),
                    SceneShotReason::PointerContext, &input);
    }

    void commit_return(const CameraProfile &profile)
    {
        commit_shot({0.5f, 0.5f}, 1.0f,
                    std::clamp(profile.zoom_out_seconds, 0.34f, 0.70f),
                    SceneShotReason::Return, nullptr);
    }

    CameraOutput step_adaptive_guard_follow(const CameraInput &input, float dt)
    {
        const float pressure = scene_follow_pressure(
            input, output_.center, output_.zoom);
        const Vec2 predicted = predicted_tracking_cursor(input, pressure);
        const SceneViewportPlan plan = scene_tracking_plan(
            input, predicted, output_.center, output_.zoom, pressure);

        if (!plan.active) {
            guard_follow_active_ = false;
            exact_hold();
            return output_;
        }

        const CameraOutput previous = output_;
        output_.center = smooth_center(
            output_.center, plan.target_center, dt,
            scene_adaptive_follow_time(output_.zoom, pressure),
            scene_adaptive_follow_max_output_speed(output_.zoom, pressure),
            output_.zoom);
        output_.state = CameraState::CatchUp;
        output_.intent_confidence = 0.92f;
        output_.urgency = pressure;

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
        return output_;
    }

    void maybe_supersede_stale_pointer_target(const CameraInput &input,
                                               const CameraProfile &profile)
    {
        if (!shot_active_ || shot_reason_ == SceneShotReason::Return ||
            !input.cursor_valid || !committed_cursor_valid_ ||
            !pointer_has_settled(input.motion_style))
            return;

        const float zoom = std::max(output_.zoom, 1.0f);
        const float moved_output =
            length(sub(input.cursor, committed_cursor_)) * zoom;
        if (moved_output <= scene_high_zoom_retarget_threshold(zoom))
            return;

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
        guard_follow_active_ = false;
        output_.center = clamp_center(output_.center, output_.zoom);
        output_.velocity = {0.0f, 0.0f};
        output_.acceleration = {0.0f, 0.0f};
        output_.state = CameraState::SmoothIdle;
        output_.intent_confidence = 1.0f;
        output_.urgency = 0.0f;
    }

    void lock_full_frame()
    {
        guard_follow_active_ = false;
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
    Vec2 pointer_velocity_scene_{0.0f, 0.0f};

    bool shot_active_ = false;
    SceneShotReason shot_reason_ = SceneShotReason::None;
    float shot_elapsed_ = 0.0f;
    float shot_duration_ = 0.40f;
    ScreenTransform shot_start_{};
    ScreenTransform shot_target_{};
    bool committed_cursor_valid_ = false;
    Vec2 committed_cursor_{0.5f, 0.5f};
    bool guard_follow_active_ = false;
    unsigned long long generation_ = 0;
};

} // namespace arzoom
