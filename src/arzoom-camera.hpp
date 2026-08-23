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
    Brake,
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

struct CameraProfile {
    float zoom_in_seconds;
    float zoom_out_seconds;
    float observe_seconds;
    float natural_frequency;
    float max_output_speed;
    float max_output_acceleration;
    float max_output_jerk;
    float catchup_scale;
    float lookahead_seconds;
    float max_lookahead_output;
    float cursor_velocity_filter_seconds;
    float settle_position_output;
    float settle_velocity_output;
};

inline CameraProfile camera_profile(CameraMotionStyle style)
{
    switch (style) {
    case CameraMotionStyle::Responsive:
        return {0.20f, 0.20f, 0.060f, 12.5f, 2.20f, 8.50f, 62.0f,
                1.55f, 0.090f, 0.065f, 0.065f, 0.007f, 0.030f};
    case CameraMotionStyle::Balanced:
        return {0.27f, 0.25f, 0.095f, 10.5f, 1.65f, 6.25f, 46.0f,
                1.60f, 0.075f, 0.055f, 0.080f, 0.007f, 0.026f};
    case CameraMotionStyle::Cinematic:
    default:
        return {0.34f, 0.30f, 0.135f, 8.5f, 1.25f, 4.60f, 32.0f,
                1.65f, 0.060f, 0.045f, 0.100f, 0.008f, 0.022f};
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

inline Vec2 clamp_magnitude(Vec2 value, float maximum)
{
    const float len = length(value);
    if (len <= std::max(maximum, 0.0f) || len <= 1.0e-8f)
        return value;
    return mul(value, maximum / len);
}

inline float smoothstep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};
        target_center_ = center_;
        zoom_ = 1.0f;
        previous_zoom_requested_ = false;
        state_ = CameraState::Rest;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        have_cursor_ = false;
        previous_cursor_ = {0.5f, 0.5f};
        filtered_cursor_velocity_ = {0.0f, 0.0f};
        previous_cursor_direction_ = {0.0f, 0.0f};
        direction_persistence_ = 0.0f;
    }

    CameraOutput step(const CameraInput &input)
    {
        const float dt = std::clamp(input.dt, 0.0f, 0.10f);
        const float configured_zoom =
            std::clamp(input.configured_zoom, 1.10f, 4.00f);
        const float safe_zone =
            std::clamp(input.safe_zone, 0.10f, 0.60f);
        const CameraProfile profile = camera_profile(input.motion_style);

        update_cursor_model(input.cursor, input.cursor_valid, dt, profile);

        const bool rising =
            input.zoom_requested && !previous_zoom_requested_;
        previous_zoom_requested_ = input.zoom_requested;

        if (rising)
            begin_activation(input, configured_zoom);

        if (!input.zoom_requested)
            return step_return(dt, profile);

        if (state_ == CameraState::Activating) {
            return step_activation(input, configured_zoom, safe_zone,
                                   dt, profile);
        }

        update_active_zoom(configured_zoom, dt, profile);

        if (input.follow_policy == CameraFollowPolicy::Fixed) {
            intent_confidence_ = 0.0f;
            urgency_ = 0.0f;
            target_center_ = clamp_center({0.5f, 0.5f}, zoom_);
            return step_confirmed_motion(target_center_, dt, profile, 0.0f);
        }

        if (!input.cursor_valid) {
            intent_confidence_ = 0.0f;
            urgency_ = 0.0f;
            if (length(velocity_) * zoom_ > profile.settle_velocity_output) {
                state_ = CameraState::Brake;
                integrate_ballistic(center_, dt, profile, 0.0f);
            } else {
                velocity_ = {0.0f, 0.0f};
                acceleration_ = {0.0f, 0.0f};
                state_ = CameraState::Rest;
            }
            return output();
        }

        if (input.follow_policy == CameraFollowPolicy::Centered) {
            target_center_ = centered_target(input.cursor, input.anchor, zoom_);
            const float distance_output =
                length(sub(target_center_, center_)) * zoom_;
            urgency_ = std::clamp(distance_output / 0.25f, 0.0f, 1.0f);
            intent_confidence_ = 1.0f;
            state_ = urgency_ > 0.55f ? CameraState::CatchUp
                                      : CameraState::Follow;
            return step_confirmed_motion(target_center_, dt, profile,
                                         urgency_);
        }

        return step_smart_follow(input, safe_zone, dt, profile);
    }

private:
    void update_cursor_model(Vec2 cursor, bool valid, float dt,
                             const CameraProfile &profile)
    {
        if (!valid) {
            filtered_cursor_velocity_ =
                mul(filtered_cursor_velocity_, std::exp(-12.0f * dt));
            direction_persistence_ =
                std::max(0.0f, direction_persistence_ - 3.0f * dt);
            return;
        }

        if (!have_cursor_ || dt <= 1.0e-6f) {
            previous_cursor_ = cursor;
            have_cursor_ = true;
            filtered_cursor_velocity_ = {0.0f, 0.0f};
            previous_cursor_direction_ = {0.0f, 0.0f};
            direction_persistence_ = 0.0f;
            return;
        }

        const Vec2 raw_velocity =
            mul(sub(cursor, previous_cursor_), 1.0f / dt);
        const float alpha =
            exponential_alpha(dt, profile.cursor_velocity_filter_seconds);
        filtered_cursor_velocity_ =
            add(filtered_cursor_velocity_,
                mul(sub(raw_velocity, filtered_cursor_velocity_), alpha));

        const Vec2 direction = normalized(raw_velocity);
        if (length(direction) > 0.0f) {
            if (length(previous_cursor_direction_) > 0.0f) {
                const float alignment =
                    dot(direction, previous_cursor_direction_);
                const float delta = alignment > 0.70f
                                        ? 3.5f * dt
                                        : -7.0f * dt;
                direction_persistence_ = std::clamp(
                    direction_persistence_ + delta, 0.0f, 1.0f);
            } else {
                direction_persistence_ = std::min(
                    1.0f, direction_persistence_ + 2.0f * dt);
            }
            previous_cursor_direction_ = normalized(
                lerp(previous_cursor_direction_, direction, 0.35f));
        } else {
            direction_persistence_ =
                std::max(0.0f, direction_persistence_ - 2.0f * dt);
        }

        previous_cursor_ = cursor;
    }

    void begin_activation(const CameraInput &input,
                          float configured_zoom)
    {
        state_ = CameraState::Activating;
        observe_seconds_ = 0.0f;
        intent_confidence_ = 1.0f;
        urgency_ = 0.0f;
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};
        activation_start_center_ = center_;
        activation_start_zoom_ = zoom_;
        activation_focus_ = input.cursor_valid ? input.cursor : center_;
        activation_initial_output_ = cursor_output_position(
            activation_focus_, center_, std::max(zoom_, 1.0f));
        activation_target_zoom_ = configured_zoom;
    }

    CameraOutput step_activation(const CameraInput &input,
                                 float configured_zoom,
                                 float safe_zone, float dt,
                                 const CameraProfile &profile)
    {
        activation_target_zoom_ = configured_zoom;
        zoom_ = smooth_scalar(zoom_, activation_target_zoom_, dt,
                              profile.zoom_in_seconds);

        Vec2 final_center{0.5f, 0.5f};
        if (input.follow_policy == CameraFollowPolicy::Centered) {
            final_center = centered_target(
                activation_focus_, input.anchor, activation_target_zoom_);
        } else if (input.follow_policy == CameraFollowPolicy::Smart) {
            final_center = smart_follow_target(
                activation_focus_, activation_start_center_, input.anchor,
                safe_zone, activation_target_zoom_);
        }

        const float span = std::max(
            activation_target_zoom_ - activation_start_zoom_, 1.0e-6f);
        const float progress = std::clamp(
            (zoom_ - activation_start_zoom_) / span, 0.0f, 1.0f);

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
            lerp(focus_preserving_center, final_center,
                 smoothstep01(progress)),
            zoom_);
        target_center_ = final_center;
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};

        if (std::fabs(zoom_ - activation_target_zoom_) <= 0.0045f ||
            progress >= 0.998f) {
            zoom_ = activation_target_zoom_;
            center_ = clamp_center(final_center, zoom_);
            target_center_ = center_;
            state_ = CameraState::Settle;
            observe_seconds_ = 0.0f;
            filtered_cursor_velocity_ = {0.0f, 0.0f};
            direction_persistence_ = 0.0f;
            if (input.cursor_valid) {
                previous_cursor_ = input.cursor;
                have_cursor_ = true;
            }
        }

        return output();
    }

    void update_active_zoom(float configured_zoom, float dt,
                            const CameraProfile &profile)
    {
        if (configured_zoom >= zoom_) {
            zoom_ = smooth_scalar(zoom_, configured_zoom, dt,
                                  profile.zoom_in_seconds);
        } else {
            const float next_zoom = smooth_scalar(
                zoom_, configured_zoom, dt, profile.zoom_out_seconds);
            zoom_ = std::max(next_zoom, minimum_zoom_for_center(center_));
        }
        center_ = clamp_center(center_, zoom_);
    }

    CameraOutput step_return(float dt,
                             const CameraProfile &profile)
    {
        intent_confidence_ = 0.0f;
        urgency_ = 0.0f;
        observe_seconds_ = 0.0f;
        state_ = CameraState::Returning;
        target_center_ = {0.5f, 0.5f};

        integrate_ballistic(target_center_, dt, profile, 0.0f);
        const float next_zoom =
            smooth_scalar(zoom_, 1.0f, dt, profile.zoom_out_seconds);
        zoom_ = std::max(next_zoom, minimum_zoom_for_center(center_));
        center_ = clamp_center(center_, zoom_);

        const float position_error =
            length(sub(center_, Vec2{0.5f, 0.5f}));
        const float output_speed =
            length(velocity_) * std::max(zoom_, 1.0f);
        if (std::fabs(zoom_ - 1.0f) < 0.001f &&
            position_error < 0.0015f &&
            output_speed < profile.settle_velocity_output) {
            center_ = {0.5f, 0.5f};
            target_center_ = center_;
            zoom_ = 1.0f;
            velocity_ = {0.0f, 0.0f};
            acceleration_ = {0.0f, 0.0f};
            state_ = CameraState::Rest;
        }
        return output();
    }

    CameraOutput step_smart_follow(const CameraInput &input,
                                   float safe_zone, float dt,
                                   const CameraProfile &profile)
    {
        const Vec2 outer_target = smart_follow_target(
            input.cursor, center_, input.anchor, safe_zone, zoom_);
        const float candidate_distance_output =
            length(sub(outer_target, center_)) * zoom_;
        const Vec2 cursor_output =
            cursor_output_position(input.cursor, center_, zoom_);
        const float edge_distance = std::max(
            std::fabs(cursor_output.x - 0.5f),
            std::fabs(cursor_output.y - 0.5f));
        const float edge_risk = std::clamp(
            (edge_distance - 0.38f) / 0.20f, 0.0f, 1.0f);
        const float distance_urgency = std::clamp(
            candidate_distance_output / 0.24f, 0.0f, 1.0f);
        urgency_ = std::max(edge_risk, distance_urgency);

        if (candidate_distance_output <= 0.0020f) {
            observe_seconds_ =
                std::max(0.0f, observe_seconds_ - dt * 3.0f);
            intent_confidence_ =
                std::max(0.0f, intent_confidence_ - dt * 5.0f);

            if (state_ == CameraState::Follow ||
                state_ == CameraState::CatchUp ||
                state_ == CameraState::Brake) {
                state_ = CameraState::Brake;
                target_center_ = center_;
                integrate_ballistic(target_center_, dt, profile, 0.0f);
                if (length(velocity_) * zoom_ <
                    profile.settle_velocity_output) {
                    velocity_ = {0.0f, 0.0f};
                    acceleration_ = {0.0f, 0.0f};
                    state_ = CameraState::Settle;
                }
            } else {
                velocity_ = {0.0f, 0.0f};
                acceleration_ = {0.0f, 0.0f};
                state_ = CameraState::Rest;
            }
            return output();
        }

        observe_seconds_ +=
            dt * (0.55f + 0.45f * direction_persistence_);
        const float output_cursor_speed =
            length(filtered_cursor_velocity_) * zoom_;
        const float distance_signal = std::clamp(
            candidate_distance_output / 0.16f, 0.0f, 1.0f);
        const float velocity_signal = std::clamp(
            output_cursor_speed / 1.50f, 0.0f, 1.0f) *
            direction_persistence_;
        const float dwell_signal = std::clamp(
            observe_seconds_ / std::max(profile.observe_seconds, 0.01f),
            0.0f, 1.0f);
        intent_confidence_ = std::clamp(
            0.40f * distance_signal + 0.18f * velocity_signal +
                0.24f * dwell_signal + 0.32f * urgency_ +
                (input.emphasis_event ? 0.70f : 0.0f),
            0.0f, 1.0f);

        if (state_ == CameraState::Rest ||
            state_ == CameraState::Settle) {
            state_ = CameraState::Observe;
        }

        const float observe_threshold = std::max(
            0.025f,
            profile.observe_seconds * (1.0f - 0.72f * urgency_));
        const bool intent_confirmed = input.emphasis_event ||
            (observe_seconds_ >= observe_threshold &&
             intent_confidence_ >= 0.50f);

        if (state_ == CameraState::Observe && !intent_confirmed) {
            velocity_ = {0.0f, 0.0f};
            acceleration_ = {0.0f, 0.0f};
            return output();
        }

        if (intent_confirmed && state_ == CameraState::Observe) {
            state_ = urgency_ > 0.55f ? CameraState::CatchUp
                                      : CameraState::Follow;
        }

        Vec2 lookahead = mul(
            filtered_cursor_velocity_,
            profile.lookahead_seconds * direction_persistence_);
        lookahead = clamp_magnitude(
            lookahead,
            profile.max_lookahead_output / std::max(zoom_, 1.0f));
        const Vec2 predicted_cursor{
            std::clamp(input.cursor.x + lookahead.x, 0.0f, 1.0f),
            std::clamp(input.cursor.y + lookahead.y, 0.0f, 1.0f),
        };

        const float settle_zone =
            std::clamp(safe_zone * 0.70f, 0.08f, safe_zone);
        target_center_ = smart_follow_target(
            predicted_cursor, center_, input.anchor,
            settle_zone, zoom_);
        return step_confirmed_motion(target_center_, dt, profile,
                                     urgency_);
    }

    CameraOutput step_confirmed_motion(Vec2 target, float dt,
                                       const CameraProfile &profile,
                                       float urgency)
    {
        const float distance_output =
            length(sub(target, center_)) * zoom_;
        const float speed_output = length(velocity_) * zoom_;
        const float available_acceleration =
            std::max(profile.max_output_acceleration, 0.1f);
        const float stopping_distance =
            speed_output * speed_output /
            (2.0f * available_acceleration);

        if (distance_output <
                std::max(0.018f, stopping_distance * 1.25f) &&
            speed_output > 0.03f) {
            state_ = CameraState::Brake;
        } else if (urgency > 0.55f) {
            state_ = CameraState::CatchUp;
        } else if (state_ != CameraState::Returning) {
            state_ = CameraState::Follow;
        }

        integrate_ballistic(target, dt, profile, urgency);

        const float remaining_output =
            length(sub(target, center_)) * zoom_;
        const float remaining_speed = length(velocity_) * zoom_;
        if (remaining_output < profile.settle_position_output &&
            remaining_speed < profile.settle_velocity_output) {
            center_ = clamp_center(target, zoom_);
            velocity_ = {0.0f, 0.0f};
            acceleration_ = {0.0f, 0.0f};
            state_ = CameraState::Settle;
            observe_seconds_ = 0.0f;
        }
        return output();
    }

    void integrate_ballistic(Vec2 target, float dt,
                             const CameraProfile &profile,
                             float urgency)
    {
        constexpr float max_substep = 1.0f / 120.0f;
        float remaining = std::clamp(dt, 0.0f, 0.10f);
        int guard = 0;
        while (remaining > 1.0e-7f && guard++ < 16) {
            const float h = std::min(remaining, max_substep);
            integrate_ballistic_substep(target, h, profile, urgency);
            remaining -= h;
        }
    }

    void integrate_ballistic_substep(Vec2 target, float dt,
                                     const CameraProfile &profile,
                                     float urgency)
    {
        const float safe_zoom = std::max(zoom_, 1.0f);
        const float urgency_scale = 1.0f +
            (profile.catchup_scale - 1.0f) *
                std::clamp(urgency, 0.0f, 1.0f);
        const float omega = profile.natural_frequency *
                            (1.0f + 0.18f * urgency);

        const Vec2 error = sub(target, center_);
        Vec2 desired_acceleration = sub(
            mul(error, omega * omega),
            mul(velocity_, 2.0f * omega));
        desired_acceleration = clamp_magnitude(
            desired_acceleration,
            profile.max_output_acceleration * urgency_scale /
                safe_zoom);

        Vec2 acceleration_delta =
            sub(desired_acceleration, acceleration_);
        acceleration_delta = clamp_magnitude(
            acceleration_delta,
            profile.max_output_jerk * urgency_scale * dt /
                safe_zoom);
        acceleration_ = add(acceleration_, acceleration_delta);

        velocity_ = add(velocity_, mul(acceleration_, dt));
        velocity_ = clamp_magnitude(
            velocity_,
            profile.max_output_speed * urgency_scale / safe_zoom);

        const Vec2 proposed = add(center_, mul(velocity_, dt));
        const Vec2 clamped = clamp_center(proposed, safe_zoom);

        if (std::fabs(clamped.x - proposed.x) > 1.0e-7f) {
            const bool pushes_out =
                (proposed.x < clamped.x && velocity_.x < 0.0f) ||
                (proposed.x > clamped.x && velocity_.x > 0.0f);
            if (pushes_out) {
                velocity_.x = 0.0f;
                acceleration_.x = 0.0f;
            }
        }
        if (std::fabs(clamped.y - proposed.y) > 1.0e-7f) {
            const bool pushes_out =
                (proposed.y < clamped.y && velocity_.y < 0.0f) ||
                (proposed.y > clamped.y && velocity_.y > 0.0f);
            if (pushes_out) {
                velocity_.y = 0.0f;
                acceleration_.y = 0.0f;
            }
        }
        center_ = clamped;
    }

    Vec2 center_{0.5f, 0.5f};
    Vec2 velocity_{0.0f, 0.0f};
    Vec2 acceleration_{0.0f, 0.0f};
    Vec2 target_center_{0.5f, 0.5f};
    float zoom_ = 1.0f;
    bool previous_zoom_requested_ = false;
    CameraState state_ = CameraState::Rest;
    float observe_seconds_ = 0.0f;
    float intent_confidence_ = 0.0f;
    float urgency_ = 0.0f;

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
};

} // namespace arzoom
