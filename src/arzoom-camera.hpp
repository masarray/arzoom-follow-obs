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
 * ArZoom's camera is intentionally gimbal-like, not spring/ballistic.
 *
 * Zoom transitions use a single minimum-jerk trajectory. Mouse follow uses a
 * cascaded low-pass destination servo. Both are monotonic for a stationary
 * target and can be re-targeted without restarting the camera motion.
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
};

inline CameraProfile camera_profile(CameraMotionStyle style)
{
    switch (style) {
    case CameraMotionStyle::Responsive:
        return {0.34f, 0.42f, 0.060f, 0.125f, 0.205f, 0.095f,
                0.85f, 0.085f, 0.0055f, 0.018f};
    case CameraMotionStyle::Cinematic:
        return {0.52f, 0.64f, 0.145f, 0.235f, 0.360f, 0.150f,
                0.55f, 0.130f, 0.0070f, 0.012f};
    case CameraMotionStyle::Balanced:
    default:
        return {0.44f, 0.56f, 0.105f, 0.195f, 0.315f, 0.125f,
                0.68f, 0.105f, 0.0060f, 0.015f};
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

/* Quintic minimum-jerk curve: zero velocity and acceleration at both ends. */
inline float minimum_jerk01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float t2 = t * t;
    const float t3 = t2 * t;
    return t3 * (10.0f + t * (-15.0f + 6.0f * t));
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
            begin_activation(input, configured_zoom, profile);
        else if (falling)
            begin_return(profile);

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
        const Vec2 measured_acceleration =
            mul(sub(measured_velocity, previous_velocity), 1.0f / dt);
        velocity_ = measured_velocity;
        acceleration_ = measured_acceleration;
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

        const Vec2 raw_velocity =
            mul(sub(cursor, previous_cursor_), 1.0f / dt);
        filtered_cursor_velocity_ = gimbal_lowpass(
            filtered_cursor_velocity_, raw_velocity, dt,
            profile.cursor_velocity_filter_seconds);

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

    void reset_gimbal_filters(Vec2 center)
    {
        desired_destination_ = center;
        destination_stage1_ = center;
        destination_stage2_ = center;
        follow_reference_center_ = center;
        follow_active_ = false;
    }

    void begin_activation(const CameraInput &input,
                          float configured_zoom,
                          const CameraProfile &profile)
    {
        (void)profile;
        state_ = CameraState::Activating;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 1.0f;
        urgency_ = 0.0f;
        activation_elapsed_ = 0.0f;
        activation_start_center_ = center_;
        activation_start_zoom_ = zoom_;
        activation_target_zoom_ = configured_zoom;
        activation_focus_ = input.cursor_valid ? input.cursor : center_;
        activation_initial_output_ = cursor_output_position(
            activation_focus_, center_, std::max(zoom_, 1.0f));
        reset_gimbal_filters(center_);
    }

    void step_activation(const CameraInput &input,
                         float configured_zoom,
                         float safe_zone, float dt,
                         const CameraProfile &profile)
    {
        activation_target_zoom_ = configured_zoom;
        activation_elapsed_ += dt;
        const float duration = std::max(profile.zoom_in_seconds, 0.05f);
        const float normalized_time =
            std::clamp(activation_elapsed_ / duration, 0.0f, 1.0f);
        const float progress = minimum_jerk01(normalized_time);

        zoom_ = activation_start_zoom_ +
                (activation_target_zoom_ - activation_start_zoom_) * progress;

        Vec2 final_center{0.5f, 0.5f};
        if (input.follow_policy == CameraFollowPolicy::Centered) {
            final_center = centered_target(
                activation_focus_, input.anchor, activation_target_zoom_);
        } else if (input.follow_policy == CameraFollowPolicy::Smart) {
            final_center = smart_follow_target(
                activation_focus_, activation_start_center_, input.anchor,
                safe_zone, activation_target_zoom_);
        }

        Vec2 focus_preserving_center = activation_start_center_;
        if (input.follow_policy != CameraFollowPolicy::Fixed) {
            const float safe_zoom = std::max(zoom_, 1.0f);
            focus_preserving_center = {
                activation_focus_.x -
                    (activation_initial_output_.x - 0.5f) / safe_zoom,
                activation_focus_.y -
                    (activation_initial_output_.y - 0.5f) / safe_zoom,
            };
        }

        center_ = clamp_center(
            lerp(focus_preserving_center, final_center, progress), zoom_);

        if (normalized_time >= 1.0f) {
            zoom_ = activation_target_zoom_;
            center_ = clamp_center(final_center, zoom_);
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
            }
        }
    }

    void begin_return(const CameraProfile &profile)
    {
        (void)profile;
        state_ = CameraState::Returning;
        return_elapsed_ = 0.0f;
        return_start_center_ = center_;
        return_start_zoom_ = zoom_;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        reset_gimbal_filters(center_);
    }

    void step_return(float dt, const CameraProfile &profile)
    {
        state_ = CameraState::Returning;
        return_elapsed_ += dt;
        const float duration = std::max(profile.zoom_out_seconds, 0.05f);
        const float normalized_time =
            std::clamp(return_elapsed_ / duration, 0.0f, 1.0f);
        const float progress = minimum_jerk01(normalized_time);

        zoom_ = return_start_zoom_ + (1.0f - return_start_zoom_) * progress;
        center_ = clamp_center(
            lerp(return_start_center_, Vec2{0.5f, 0.5f}, progress),
            std::max(zoom_, 1.0f));

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
            state_ = CameraState::Rest;
            return;
        }

        if (input.follow_policy == CameraFollowPolicy::Centered) {
            follow_active_ = true;
            follow_reference_center_ = center_;
            const Vec2 target = centered_target(
                input.cursor, input.anchor, zoom_);
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

        if (!follow_active_) {
            if (candidate_distance_output <= 0.0020f) {
                observe_seconds_ = 0.0f;
                intent_confidence_ = 0.0f;
                state_ = CameraState::Rest;
                urgency_ = gimbal_lowpass(
                    urgency_, 0.0f, dt,
                    profile.urgency_filter_seconds);
                reset_gimbal_filters(center_);
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
            const float persistence_signal = direction_persistence_;
            intent_confidence_ = std::clamp(
                0.46f * distance_signal +
                    0.30f * dwell_signal +
                    0.12f * persistence_signal +
                    0.26f * urgency_ +
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
        }

        /*
         * During one continuous follow shot the reference center stays fixed.
         * This is critical: a stationary mouse therefore produces a stationary
         * destination instead of a target that collapses as the camera moves.
         * If the user moves again mid-shot, only the destination changes; the
         * camera filters keep their state and bend the path smoothly.
         */
        const float settle_zone =
            std::clamp(safe_zone * 0.58f, 0.08f, safe_zone);
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

    Vec2 activation_focus_{0.5f, 0.5f};
    Vec2 activation_start_center_{0.5f, 0.5f};
    Vec2 activation_initial_output_{0.5f, 0.5f};
    float activation_start_zoom_ = 1.0f;
    float activation_target_zoom_ = 2.0f;
    float activation_elapsed_ = 0.0f;

    Vec2 return_start_center_{0.5f, 0.5f};
    float return_start_zoom_ = 1.0f;
    float return_elapsed_ = 0.0f;
};

} // namespace arzoom
