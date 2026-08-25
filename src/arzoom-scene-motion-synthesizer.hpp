#pragma once

#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

/*
 * Scene Camera kinematic motion synthesizer
 * =========================================
 *
 * This layer owns HOW an already-selected viewport target is reached. It has no
 * pointer semantics, no scene mapping and no presentation-zone decisions.
 *
 * The state is explicitly position + velocity + acceleration. A critically
 * damped target acceleration is jerk-limited before integration, so a changing
 * target cannot instantly change camera velocity or acceleration. The same
 * state survives TRACK -> SETTLE handoff; there is no zero-velocity restart.
 *
 * All limits are expressed in output-space units and converted to scene-space
 * by the current zoom. The integrator uses bounded fixed substeps for stable,
 * near frame-rate-independent behavior at 30/60/120/144 fps. State/work are
 * O(1); no motion history is allocated.
 */

struct SceneMotionLimits {
    float natural_frequency = 9.0f;
    float max_speed_output = 3.0f;
    float max_acceleration_output = 18.0f;
    float max_jerk_output = 120.0f;
    float settle_position_output = 0.0018f;
    float settle_velocity_output = 0.012f;
    float settle_acceleration_output = 0.20f;
};

inline Vec2 scene_clamp_magnitude(Vec2 value, float maximum)
{
    const float limit = std::max(maximum, 0.0f);
    const float magnitude = length(value);
    if (magnitude <= limit || magnitude <= 1.0e-8f)
        return value;
    return mul(value, limit / magnitude);
}

inline float scene_smoothstep01(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline SceneMotionLimits scene_motion_limits(CameraMotionStyle style,
                                              float zoom,
                                              float pressure,
                                              float distance_output)
{
    const float p = scene_smoothstep01(pressure);
    const float z = std::clamp((std::max(zoom, 1.0f) - 2.60f) / 1.40f,
                               0.0f, 1.0f);
    const float d = scene_smoothstep01(
        std::clamp((distance_output - 0.04f) / 0.42f, 0.0f, 1.0f));
    const float urgency = std::clamp(std::max(p, 0.72f * d) + 0.18f * z,
                                     0.0f, 1.0f);

    SceneMotionLimits limits;
    switch (style) {
    case CameraMotionStyle::Responsive:
        limits.natural_frequency = 10.5f + 2.8f * urgency;
        limits.max_speed_output = 3.20f + 2.40f * urgency;
        limits.max_acceleration_output = 20.0f + 14.0f * urgency;
        limits.max_jerk_output = 135.0f + 105.0f * urgency;
        break;
    case CameraMotionStyle::Cinematic:
        limits.natural_frequency = 7.4f + 2.2f * urgency;
        limits.max_speed_output = 2.25f + 1.85f * urgency;
        limits.max_acceleration_output = 13.0f + 10.0f * urgency;
        limits.max_jerk_output = 82.0f + 78.0f * urgency;
        break;
    case CameraMotionStyle::Balanced:
    default:
        limits.natural_frequency = 8.8f + 2.5f * urgency;
        limits.max_speed_output = 2.75f + 2.15f * urgency;
        limits.max_acceleration_output = 17.0f + 12.0f * urgency;
        limits.max_jerk_output = 110.0f + 90.0f * urgency;
        break;
    }

    /* High zoom needs decisiveness, not discontinuity. The additional speed is
     * modest because output-space limits already make behavior zoom invariant. */
    limits.max_speed_output *= 1.0f + 0.10f * z;
    limits.max_acceleration_output *= 1.0f + 0.08f * z;
    limits.settle_position_output = 0.0018f;
    limits.settle_velocity_output = 0.012f;
    limits.settle_acceleration_output = 0.20f;
    return limits;
}

struct SceneMotionSample {
    Vec2 center{0.5f, 0.5f};
    Vec2 velocity{0.0f, 0.0f};
    Vec2 acceleration{0.0f, 0.0f};
    bool settled = true;
};

class SceneKinematicMotion {
public:
    void reset(Vec2 center = {0.5f, 0.5f})
    {
        center_ = center;
        velocity_ = {0.0f, 0.0f};
        acceleration_ = {0.0f, 0.0f};
    }

    void adopt(Vec2 center, Vec2 velocity, Vec2 acceleration, float zoom)
    {
        center_ = clamp_center(center, std::max(zoom, 1.0f));
        velocity_ = velocity;
        acceleration_ = acceleration;
    }

    SceneMotionSample sample() const
    {
        return {center_, velocity_, acceleration_, false};
    }

    SceneMotionSample step(Vec2 target_center, float zoom, float dt,
                           const SceneMotionLimits &limits)
    {
        const float safe_zoom = std::max(zoom, 1.0f);
        target_center = clamp_center(target_center, safe_zoom);
        const float total_dt = std::clamp(dt, 0.0f, 0.10f);
        if (total_dt <= 1.0e-7f)
            return current_sample(target_center, safe_zoom, limits);

        /* Bounded substeps make the jerk-limited second-order controller stable
         * and visually consistent across common OBS frame rates. */
        constexpr float max_substep = 1.0f / 120.0f;
        const int steps = std::clamp(
            static_cast<int>(std::ceil(total_dt / max_substep)), 1, 12);
        const float h = total_dt / static_cast<float>(steps);

        for (int i = 0; i < steps; ++i)
            integrate_substep(target_center, safe_zoom, h, limits);

        SceneMotionSample result = current_sample(
            target_center, safe_zoom, limits);
        if (result.settled) {
            center_ = target_center;
            velocity_ = {0.0f, 0.0f};
            acceleration_ = {0.0f, 0.0f};
            result.center = center_;
            result.velocity = velocity_;
            result.acceleration = acceleration_;
        }
        return result;
    }

private:
    void integrate_substep(Vec2 target_center, float zoom, float dt,
                           const SceneMotionLimits &limits)
    {
        const Vec2 error_output = mul(sub(target_center, center_), zoom);
        Vec2 velocity_output = mul(velocity_, zoom);
        Vec2 acceleration_output = mul(acceleration_, zoom);

        const float omega = std::max(limits.natural_frequency, 0.1f);
        Vec2 desired_acceleration = sub(
            mul(error_output, omega * omega),
            mul(velocity_output, 2.0f * omega));
        desired_acceleration = scene_clamp_magnitude(
            desired_acceleration, limits.max_acceleration_output);

        const Vec2 acceleration_delta = sub(
            desired_acceleration, acceleration_output);
        const Vec2 limited_delta = scene_clamp_magnitude(
            acceleration_delta,
            std::max(limits.max_jerk_output, 1.0f) * dt);
        acceleration_output = add(acceleration_output, limited_delta);
        acceleration_output = scene_clamp_magnitude(
            acceleration_output, limits.max_acceleration_output);

        velocity_output = add(
            velocity_output, mul(acceleration_output, dt));
        velocity_output = scene_clamp_magnitude(
            velocity_output, limits.max_speed_output);

        const Vec2 proposed = add(
            center_, mul(velocity_output, dt / zoom));
        const Vec2 clamped = clamp_center(proposed, zoom);

        /* Physical scene edges are hard constraints. Cancel only the component
         * that attempted to travel through the wall; tangential motion survives. */
        if (std::fabs(clamped.x - proposed.x) > 1.0e-7f) {
            velocity_output.x = 0.0f;
            acceleration_output.x = 0.0f;
        }
        if (std::fabs(clamped.y - proposed.y) > 1.0e-7f) {
            velocity_output.y = 0.0f;
            acceleration_output.y = 0.0f;
        }

        center_ = clamped;
        velocity_ = mul(velocity_output, 1.0f / zoom);
        acceleration_ = mul(acceleration_output, 1.0f / zoom);
    }

    SceneMotionSample current_sample(Vec2 target_center, float zoom,
                                     const SceneMotionLimits &limits) const
    {
        const float position_error =
            length(sub(target_center, center_)) * zoom;
        const float speed = length(velocity_) * zoom;
        const float acceleration = length(acceleration_) * zoom;
        const bool settled =
            position_error <= limits.settle_position_output &&
            speed <= limits.settle_velocity_output &&
            acceleration <= limits.settle_acceleration_output;
        return {center_, velocity_, acceleration_, settled};
    }

    Vec2 center_{0.5f, 0.5f};
    Vec2 velocity_{0.0f, 0.0f};
    Vec2 acceleration_{0.0f, 0.0f};
};

} // namespace arzoom
