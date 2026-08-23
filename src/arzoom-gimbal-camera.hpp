#pragma once

#include "arzoom-math.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

enum class CameraState {
    Rest,
    Activating,
    Observe,
    Follow,
    CatchUp,
    Settle,
    Returning,
};

enum class CameraFollowPolicy {
    Smart,
    Centered,
    Fixed,
};

enum class CameraMotionStyle {
    Cinematic,
    Balanced,
    Responsive,
};

/*
 * ArZoom Smart Gimbal Camera
 * -------------------------
 * The visual contract is steadiness, not physical simulation.
 *
 * - Zoom transitions interpolate the affine screen transform with a quintic
 *   minimum-jerk progress curve. Every source pixel therefore travels on a
 *   straight screen-space line between the start and end frames.
 * - Smart Follow uses an explanation lock plus cascaded low-pass destination
 *   filters. Local circles/repeated pointing stay still; real relocation bends
 *   the destination continuously without restarting motion.
 */
struct CameraProfile {
    float zoom_in_seconds;
    float zoom_out_seconds;
    float observe_seconds;
    float destination_filter_seconds;
    float camera_filter_seconds;
    float urgency_filter_seconds;
    float urgency_response_gain;
    float cursor_velocity_filter_seconds;
    float settle_position_output;
    float settle_velocity_output;
    float explanation_radius_output;
    float explanation_orbit_radius_output;
    float explanation_coherence_limit;
};

inline CameraProfile camera_profile(CameraMotionStyle style)
{
    switch (style) {
    case CameraMotionStyle::Responsive:
        return {0.34f, 0.42f, 0.060f, 0.125f, 0.205f, 0.095f,
                0.85f, 0.085f, 0.0055f, 0.018f, 0.125f, 0.180f, 0.42f};
    case CameraMotionStyle::Cinematic:
        return {0.52f, 0.64f, 0.145f, 0.235f, 0.360f, 0.150f,
                0.55f, 0.130f, 0.0070f, 0.012f, 0.175f, 0.245f, 0.48f};
    case CameraMotionStyle::Balanced:
    default:
        return {0.44f, 0.56f, 0.105f, 0.195f, 0.315f, 0.125f,
                0.68f, 0.105f, 0.0060f, 0.015f, 0.155f, 0.220f, 0.45f};
    }
}

struct CameraInput {
    float dt = 1.0f / 60.0f;
    Vec2 cursor{0.5f, 0.5f};
    bool cursor_valid = false;
    bool zoom_requested = false;
    float configured_zoom = 2.0f;
    Vec2 anchor{0.5f, 0.45f};
    float safe_zone = 0.28f;
    CameraFollowPolicy follow_policy = CameraFollowPolicy::Smart;
    CameraMotionStyle motion_style = CameraMotionStyle::Balanced;
    bool emphasis_event = false;
};

struct CameraOutput {
    Vec2 center{0.5f, 0.5f};
    float zoom = 1.0f;
    Vec2 velocity{0.0f, 0.0f};
    Vec2 acceleration{0.0f, 0.0f};
    CameraState state = CameraState::Rest;
    float intent_confidence = 0.0f;
    float urgency = 0.0f;
};

inline float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

inline Vec2 lerp(Vec2 a, Vec2 b, float t)
{
    return add(a, mul(sub(b, a), t));
}

inline Vec2 normalized(Vec2 value)
{
    const float len = length(value);
    return len > 1.0e-6f ? mul(value, 1.0f / len)
                          : Vec2{0.0f, 0.0f};
}

inline float gimbal_alpha(float dt, float time_constant)
{
    const float safe_dt = std::clamp(dt, 0.0f, 0.10f);
    const float tau = std::max(time_constant, 0.001f);
    return 1.0f - std::exp(-safe_dt / tau);
}

inline Vec2 gimbal_lowpass(Vec2 current, Vec2 target, float dt,
                           float time_constant)
{
    return lerp(current, target, gimbal_alpha(dt, time_constant));
}

inline float gimbal_lowpass(float current, float target, float dt,
                            float time_constant)
{
    return current + (target - current) *
                         gimbal_alpha(dt, time_constant);
}

/* Quintic minimum-jerk: zero velocity and acceleration at both ends. */
inline float minimum_jerk01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float t2 = t * t;
    const float t3 = t2 * t;
    return t3 * (10.0f + t * (-15.0f + 6.0f * t));
}

/*
 * Screen transform for a normalized source coordinate s:
 *     output = scale * s + offset
 *
 * Interpolating scale and offset with the SAME scalar progress guarantees
 * every source pixel moves on a straight line in output space.
 */
struct ScreenTransform {
    float scale = 1.0f;
    Vec2 offset{0.0f, 0.0f};
};

inline ScreenTransform screen_transform(Vec2 center, float zoom)
{
    const float scale = std::max(zoom, 1.0f);
    return {
        scale,
        {0.5f - scale * center.x, 0.5f - scale * center.y},
    };
}

inline Vec2 transform_center(const ScreenTransform &transform)
{
    const float scale = std::max(transform.scale, 1.0e-6f);
    return {
        (0.5f - transform.offset.x) / scale,
        (0.5f - transform.offset.y) / scale,
    };
}

inline ScreenTransform lerp_transform(const ScreenTransform &a,
                                      const ScreenTransform &b, float t)
{
    return {
        a.scale + (b.scale - a.scale) * t,
        lerp(a.offset, b.offset, t),
    };
}

class SmartCamera {
public:
    CameraOutput output() const
    {
        return {center_, zoom_, velocity_, acceleration_, state_,
                intent_confidence_, urgency_};
    }

    void reset()
    {
        center_ = {0.5f, 0.5f};
        zoom_ = 1.0f;
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};
        previous_zoom_requested_ = false;
        state_ = CameraState::Rest;

        observe_seconds_ = 0.0f;
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        follow_active_ = false;
        follow_reference_center_ = center_;
        desired_destination_ = center_;
        destination_stage1_ = center_;
        destination_stage2_ = center_;

        have_cursor_ = false;
        previous_cursor_ = {0.5f, 0.5f};
        filtered_cursor_velocity_ = {0.0f, 0.0f};
        previous_cursor_direction_ = {0.0f, 0.0f};
        direction_persistence_ = 0.0f;

        explanation_anchor_valid_ = false;
        explanation_anchor_cursor_ = {0.5f, 0.5f};
        explanation_path_source_ = 0.0f;
        cursor_still_seconds_ = 0.0f;

        activation_elapsed_ = 0.0f;
        return_elapsed_ = 0.0f;
    }

    CameraOutput step(const CameraInput &input)
    {
        const float dt = std::clamp(input.dt, 0.0f, 0.10f);
        const float configured_zoom =
            std::clamp(input.configured_zoom, 1.10f, 4.00f);
        const float safe_zone =
            std::clamp(input.safe_zone, 0.10f, 0.60f);
        const CameraProfile profile = camera_profile(input.motion_style);

        const Vec2 previous_center = center_;
        const Vec2 previous_velocity = velocity_;

        update_cursor_model(input.cursor, input.cursor_valid, dt, profile);

        const bool rising =
            input.zoom_requested && !previous_zoom_requested_;
        const bool falling =
            !input.zoom_requested && previous_zoom_requested_;
        previous_zoom_requested_ = input.zoom_requested;

        if (rising)
            begin_activation(input, configured_zoom, safe_zone);
        else if (falling)
            begin_return();

        if (!input.zoom_requested) {
            if (state_ == CameraState::Returning)
                step_return(dt, profile);
            else
                lock_full_frame();
        } else if (state_ == CameraState::Activating) {
            step_activation(input, configured_zoom, safe_zone, dt, profile);
        } else {
            update_active_zoom(configured_zoom, dt, profile);
            step_active_follow(input, safe_zone, dt, profile);
        }

        update_motion_diagnostics(previous_center, previous_velocity, dt);
        return output();
    }

private:
    void update_motion_diagnostics(Vec2 previous_center,
                                   Vec2 previous_velocity, float dt)
    {
        if (dt <= 1.0e-6f || state_ == CameraState::Rest) {
            velocity_ = {0.0f, 0.0f};
            acceleration_ = {0.0f, 0.0f};
            return;
        }
        const Vec2 measured_velocity =
            mul(sub(center_, previous_center), 1.0f / dt);
        velocity_ = measured_velocity;
        acceleration_ =
            mul(sub(measured_velocity, previous_velocity), 1.0f / dt);
    }

    void update_cursor_model(Vec2 cursor, bool valid, float dt,
                             const CameraProfile &profile)
    {
        if (!valid) {
            filtered_cursor_velocity_ = gimbal_lowpass(
                filtered_cursor_velocity_, {0.0f, 0.0f}, dt,
                profile.cursor_velocity_filter_seconds);
            direction_persistence_ =
                std::max(0.0f, direction_persistence_ - 3.0f * dt);
            cursor_still_seconds_ = 0.0f;
            return;
        }

        if (!have_cursor_ || dt <= 1.0e-6f) {
            previous_cursor_ = cursor;
            filtered_cursor_velocity_ = {0.0f, 0.0f};
            previous_cursor_direction_ = {0.0f, 0.0f};
            direction_persistence_ = 0.0f;
            have_cursor_ = true;
            return;
        }

        const Vec2 delta = sub(cursor, previous_cursor_);
        explanation_path_source_ += length(delta);
        const Vec2 raw_velocity = mul(delta, 1.0f / dt);
        filtered_cursor_velocity_ = gimbal_lowpass(
            filtered_cursor_velocity_, raw_velocity, dt,
            profile.cursor_velocity_filter_seconds);

        const float output_cursor_speed =
            length(filtered_cursor_velocity_) * std::max(zoom_, 1.0f);
        if (output_cursor_speed < 0.018f)
            cursor_still_seconds_ += dt;
        else
            cursor_still_seconds_ = 0.0f;

        const Vec2 direction = normalized(raw_velocity);
        if (length(direction) > 0.0f) {
            if (length(previous_cursor_direction_) > 0.0f) {
                const float alignment =
                    dot(direction, previous_cursor_direction_);
                direction_persistence_ = std::clamp(
                    direction_persistence_ +
                        (alignment > 0.72f ? 2.8f : -6.5f) * dt,
                    0.0f, 1.0f);
            } else {
                direction_persistence_ =
                    std::min(1.0f, direction_persistence_ + 1.8f * dt);
            }
            previous_cursor_direction_ = normalized(
                lerp(previous_cursor_direction_, direction, 0.25f));
        } else {
            direction_persistence_ =
                std::max(0.0f, direction_persistence_ - 2.5f * dt);
        }

        previous_cursor_ = cursor;
    }

    void arm_explanation_lock(Vec2 cursor)
    {
        explanation_anchor_cursor_ = cursor;
        explanation_anchor_valid_ = true;
        explanation_path_source_ = 0.0f;
        cursor_still_seconds_ = 0.0f;
    }

    void reset_gimbal_filters(Vec2 center)
    {
        desired_destination_ = center;
        destination_stage1_ = center;
        destination_stage2_ = center;
        follow_reference_center_ = center;
        follow_active_ = false;
    }

    Vec2 final_activation_center(const CameraInput &input,
                                 float target_zoom,
                                 float safe_zone) const
    {
        if (input.follow_policy == CameraFollowPolicy::Centered)
            return centered_target(activation_focus_, input.anchor, target_zoom);
        if (input.follow_policy == CameraFollowPolicy::Smart) {
            return smart_follow_target(
                activation_focus_, activation_start_center_, input.anchor,
                safe_zone, target_zoom);
        }
        return clamp_center({0.5f, 0.5f}, target_zoom);
    }

    void begin_activation(const CameraInput &input,
                          float configured_zoom, float safe_zone)
    {
        state_ = CameraState::Activating;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 1.0f;
        urgency_ = 0.0f;
        activation_elapsed_ = 0.0f;
        activation_start_center_ = center_;
        activation_start_zoom_ = zoom_;
        activation_target_zoom_ = configured_zoom;
        activation_focus_ = input.cursor_valid ? input.cursor : center_;
        activation_start_transform_ =
            screen_transform(activation_start_center_, activation_start_zoom_);
        const Vec2 final_center =
            final_activation_center(input, configured_zoom, safe_zone);
        activation_target_transform_ =
            screen_transform(final_center, configured_zoom);
        reset_gimbal_filters(center_);
        explanation_anchor_valid_ = false;
    }

    void step_activation(const CameraInput &input,
                         float configured_zoom,
                         float safe_zone, float dt,
                         const CameraProfile &profile)
    {
        /* Recompute only the final scale if the user changes zoom during the
         * transition; the latched focus remains unchanged. */
        if (std::fabs(configured_zoom - activation_target_zoom_) > 0.0001f) {
            activation_target_zoom_ = configured_zoom;
            const Vec2 final_center =
                final_activation_center(input, configured_zoom, safe_zone);
            activation_target_transform_ =
                screen_transform(final_center, configured_zoom);
        }

        activation_elapsed_ += dt;
        const float duration = std::max(profile.zoom_in_seconds, 0.05f);
        const float normalized_time =
            std::clamp(activation_elapsed_ / duration, 0.0f, 1.0f);
        const float progress = minimum_jerk01(normalized_time);

        const ScreenTransform transform = lerp_transform(
            activation_start_transform_, activation_target_transform_, progress);
        zoom_ = transform.scale;
        center_ = clamp_center(transform_center(transform), zoom_);

        if (normalized_time >= 1.0f) {
            zoom_ = activation_target_transform_.scale;
            center_ = clamp_center(
                transform_center(activation_target_transform_), zoom_);
            state_ = CameraState::Settle;
            observe_seconds_ = 0.0f;
            intent_confidence_ = 0.0f;
            urgency_ = 0.0f;
            reset_gimbal_filters(center_);
            if (input.cursor_valid) {
                previous_cursor_ = input.cursor;
                filtered_cursor_velocity_ = {0.0f, 0.0f};
                previous_cursor_direction_ = {0.0f, 0.0f};
                direction_persistence_ = 0.0f;
                have_cursor_ = true;
                arm_explanation_lock(input.cursor);
            }
        }
    }

    void begin_return()
    {
        state_ = CameraState::Returning;
        return_elapsed_ = 0.0f;
        return_start_transform_ = screen_transform(center_, zoom_);
        return_target_transform_ = screen_transform({0.5f, 0.5f}, 1.0f);
        observe_seconds_ = 0.0f;
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        reset_gimbal_filters(center_);
        explanation_anchor_valid_ = false;
    }

    void step_return(float dt, const CameraProfile &profile)
    {
        state_ = CameraState::Returning;
        return_elapsed_ += dt;
        const float duration = std::max(profile.zoom_out_seconds, 0.05f);
        const float normalized_time =
            std::clamp(return_elapsed_ / duration, 0.0f, 1.0f);
        const float progress = minimum_jerk01(normalized_time);

        const ScreenTransform transform = lerp_transform(
            return_start_transform_, return_target_transform_, progress);
        zoom_ = transform.scale;
        center_ = clamp_center(transform_center(transform), zoom_);

        if (normalized_time >= 1.0f)
            lock_full_frame();
    }

    void lock_full_frame()
    {
        center_ = {0.5f, 0.5f};
        zoom_ = 1.0f;
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};
        state_ = CameraState::Rest;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        reset_gimbal_filters(center_);
        explanation_anchor_valid_ = false;
    }

    void update_active_zoom(float configured_zoom, float dt,
                            const CameraProfile &profile)
    {
        if (std::fabs(configured_zoom - zoom_) < 0.0001f)
            return;
        const float tau = configured_zoom > zoom_
                              ? profile.zoom_in_seconds * 0.45f
                              : profile.zoom_out_seconds * 0.45f;
        float next_zoom = gimbal_lowpass(zoom_, configured_zoom, dt, tau);
        if (next_zoom < zoom_)
            next_zoom = std::max(next_zoom,
                                 minimum_zoom_for_center(center_));
        zoom_ = std::clamp(next_zoom, 1.0f, 4.0f);
        center_ = clamp_center(center_, zoom_);
        destination_stage1_ = clamp_center(destination_stage1_, zoom_);
        destination_stage2_ = clamp_center(destination_stage2_, zoom_);
        desired_destination_ = clamp_center(desired_destination_, zoom_);
        follow_reference_center_ = clamp_center(
            follow_reference_center_, zoom_);
    }

    void step_active_follow(const CameraInput &input,
                            float safe_zone, float dt,
                            const CameraProfile &profile)
    {
        if (input.follow_policy == CameraFollowPolicy::Fixed) {
            intent_confidence_ = 0.0f;
            urgency_ = gimbal_lowpass(
                urgency_, 0.0f, dt, profile.urgency_filter_seconds);
            step_gimbal_to(clamp_center({0.5f, 0.5f}, zoom_),
                           dt, profile, urgency_);
            return;
        }

        if (!input.cursor_valid) {
            observe_seconds_ = 0.0f;
            intent_confidence_ = 0.0f;
            urgency_ = gimbal_lowpass(
                urgency_, 0.0f, dt, profile.urgency_filter_seconds);
            reset_gimbal_filters(center_);
            explanation_anchor_valid_ = false;
            state_ = CameraState::Rest;
            return;
        }

        if (!explanation_anchor_valid_ && !follow_active_)
            arm_explanation_lock(input.cursor);

        if (input.follow_policy == CameraFollowPolicy::Centered) {
            follow_active_ = true;
            follow_reference_center_ = center_;
            const Vec2 target = centered_target(input.cursor, input.anchor, zoom_);
            const float distance_output =
                length(sub(target, center_)) * zoom_;
            const float raw_urgency = std::clamp(
                distance_output / 0.32f, 0.0f, 1.0f);
            urgency_ = gimbal_lowpass(
                urgency_, raw_urgency, dt,
                profile.urgency_filter_seconds);
            intent_confidence_ = 1.0f;
            step_gimbal_to(target, dt, profile, urgency_);
            return;
        }

        step_smart_follow(input, safe_zone, dt, profile);
    }

    bool explanation_gesture(const CameraInput &input,
                             const CameraProfile &profile,
                             float edge_risk) const
    {
        if (!explanation_anchor_valid_ || input.emphasis_event)
            return false;

        const float safe_zoom = std::max(zoom_, 1.0f);
        const float net_output =
            length(sub(input.cursor, explanation_anchor_cursor_)) * safe_zoom;
        const float path_output =
            explanation_path_source_ * safe_zoom;
        const float coherence =
            path_output > 0.001f ? net_output / path_output : 1.0f;

        const bool inside_local_area =
            net_output <= profile.explanation_radius_output;
        const bool orbiting_same_area =
            net_output <= profile.explanation_orbit_radius_output &&
            path_output >= profile.explanation_radius_output * 0.70f &&
            coherence <= profile.explanation_coherence_limit;

        /* Safety wins if the pointer is truly close to being lost. */
        return edge_risk < 0.78f &&
               (inside_local_area || orbiting_same_area);
    }

    void step_smart_follow(const CameraInput &input,
                           float safe_zone, float dt,
                           const CameraProfile &profile)
    {
        const Vec2 detection_target = smart_follow_target(
            input.cursor, center_, input.anchor, safe_zone, zoom_);
        const float candidate_distance_output =
            length(sub(detection_target, center_)) * zoom_;
        const Vec2 cursor_output =
            cursor_output_position(input.cursor, center_, zoom_);
        const float edge_distance = std::max(
            std::fabs(cursor_output.x - 0.5f),
            std::fabs(cursor_output.y - 0.5f));
        const float edge_risk = std::clamp(
            (edge_distance - 0.39f) / 0.16f, 0.0f, 1.0f);
        const float distance_urgency = std::clamp(
            candidate_distance_output / 0.30f, 0.0f, 1.0f);
        const float raw_urgency = std::max(edge_risk, distance_urgency);
        urgency_ = gimbal_lowpass(
            urgency_, raw_urgency, dt, profile.urgency_filter_seconds);

        if (!follow_active_ && explanation_gesture(input, profile, edge_risk)) {
            observe_seconds_ = 0.0f;
            intent_confidence_ = 0.0f;
            urgency_ = gimbal_lowpass(
                urgency_, 0.0f, dt, profile.urgency_filter_seconds);
            reset_gimbal_filters(center_);
            state_ = CameraState::Rest;
            return;
        }

        if (!follow_active_) {
            if (candidate_distance_output <= 0.0020f) {
                observe_seconds_ = 0.0f;
                intent_confidence_ = 0.0f;
                state_ = CameraState::Rest;
                urgency_ = gimbal_lowpass(
                    urgency_, 0.0f, dt,
                    profile.urgency_filter_seconds);
                reset_gimbal_filters(center_);
                if (cursor_still_seconds_ > 0.45f)
                    arm_explanation_lock(input.cursor);
                return;
            }

            state_ = CameraState::Observe;
            observe_seconds_ += dt *
                (0.72f + 0.28f * direction_persistence_);

            const float distance_signal = std::clamp(
                candidate_distance_output / 0.18f, 0.0f, 1.0f);
            const float dwell_signal = std::clamp(
                observe_seconds_ /
                    std::max(profile.observe_seconds, 0.01f),
                0.0f, 1.0f);
            intent_confidence_ = std::clamp(
                0.48f * distance_signal +
                    0.28f * dwell_signal +
                    0.12f * direction_persistence_ +
                    0.25f * urgency_ +
                    (input.emphasis_event ? 0.70f : 0.0f),
                0.0f, 1.0f);

            const float observe_threshold = std::max(
                0.030f,
                profile.observe_seconds *
                    (1.0f - 0.58f * urgency_));
            const bool confirmed = input.emphasis_event ||
                (observe_seconds_ >= observe_threshold &&
                 intent_confidence_ >= 0.50f);
            if (!confirmed)
                return;

            follow_active_ = true;
            follow_reference_center_ = center_;
            desired_destination_ = center_;
            destination_stage1_ = center_;
            destination_stage2_ = center_;
            explanation_anchor_valid_ = false;
        }

        const float settle_zone =
            std::clamp(safe_zone * 0.62f, 0.08f, safe_zone);
        const Vec2 target = smart_follow_target(
            input.cursor, follow_reference_center_, input.anchor,
            settle_zone, zoom_);

        state_ = urgency_ > 0.72f
                     ? CameraState::CatchUp
                     : CameraState::Follow;
        step_gimbal_to(target, dt, profile, urgency_);

        const float target_error_output =
            length(sub(target, center_)) * zoom_;
        const float stage1_error_output =
            length(sub(target, destination_stage1_)) * zoom_;
        const float stage2_error_output =
            length(sub(destination_stage1_, destination_stage2_)) * zoom_;
        const float speed_output = length(velocity_) * zoom_;
        const float cursor_speed_output =
            length(filtered_cursor_velocity_) * zoom_;

        if (target_error_output < profile.settle_position_output &&
            stage1_error_output < profile.settle_position_output &&
            stage2_error_output < profile.settle_position_output &&
            speed_output < profile.settle_velocity_output &&
            cursor_speed_output < 0.035f) {
            center_ = clamp_center(target, zoom_);
            reset_gimbal_filters(center_);
            observe_seconds_ = 0.0f;
            intent_confidence_ = 0.0f;
            urgency_ = 0.0f;
            state_ = CameraState::Rest;
            arm_explanation_lock(input.cursor);
        }
    }

    void step_gimbal_to(Vec2 target, float dt,
                        const CameraProfile &profile,
                        float urgency)
    {
        desired_destination_ = clamp_center(target, zoom_);
        const float smooth_urgency = std::clamp(urgency, 0.0f, 1.0f);
        const float response_scale =
            1.0f + profile.urgency_response_gain * smooth_urgency;
        const float destination_tau =
            profile.destination_filter_seconds / response_scale;
        const float camera_tau =
            profile.camera_filter_seconds /
            (1.0f + 0.60f * profile.urgency_response_gain *
                         smooth_urgency);

        destination_stage1_ = gimbal_lowpass(
            destination_stage1_, desired_destination_, dt,
            destination_tau);
        destination_stage2_ = gimbal_lowpass(
            destination_stage2_, destination_stage1_, dt,
            destination_tau * 0.72f);
        center_ = clamp_center(
            gimbal_lowpass(center_, destination_stage2_, dt,
                           camera_tau),
            zoom_);
    }

    Vec2 center_{0.5f, 0.5f};
    float zoom_ = 1.0f;
    Vec2 velocity_{0.0f, 0.0f};
    Vec2 acceleration_{0.0f, 0.0f};
    bool previous_zoom_requested_ = false;
    CameraState state_ = CameraState::Rest;

    float observe_seconds_ = 0.0f;
    float intent_confidence_ = 0.0f;
    float urgency_ = 0.0f;

    bool follow_active_ = false;
    Vec2 follow_reference_center_{0.5f, 0.5f};
    Vec2 desired_destination_{0.5f, 0.5f};
    Vec2 destination_stage1_{0.5f, 0.5f};
    Vec2 destination_stage2_{0.5f, 0.5f};

    bool have_cursor_ = false;
    Vec2 previous_cursor_{0.5f, 0.5f};
    Vec2 filtered_cursor_velocity_{0.0f, 0.0f};
    Vec2 previous_cursor_direction_{0.0f, 0.0f};
    float direction_persistence_ = 0.0f;

    bool explanation_anchor_valid_ = false;
    Vec2 explanation_anchor_cursor_{0.5f, 0.5f};
    float explanation_path_source_ = 0.0f;
    float cursor_still_seconds_ = 0.0f;

    Vec2 activation_focus_{0.5f, 0.5f};
    Vec2 activation_start_center_{0.5f, 0.5f};
    float activation_start_zoom_ = 1.0f;
    float activation_target_zoom_ = 2.0f;
    float activation_elapsed_ = 0.0f;
    ScreenTransform activation_start_transform_{};
    ScreenTransform activation_target_transform_{};

    float return_elapsed_ = 0.0f;
    ScreenTransform return_start_transform_{};
    ScreenTransform return_target_transform_{};
};

} // namespace arzoom
