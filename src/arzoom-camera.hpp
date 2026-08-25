#pragma once

#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cmath>

namespace arzoom {

/*
 * Presenter-aware Smart Camera coordinator
 * ----------------------------------------
 *
 * The accepted SmartCamera remains the one semantic follow engine. Scene Camera
 * P4.1 can opt into three presentation contracts:
 *
 * 1. Active Zoom +/- is a joint affine viewport shot. Scale and center move
 *    together with one minimum-jerk trajectory toward the current pointer, so
 *    zoom-out never has to freeze merely to preserve an old off-centre frame.
 * 2. A pointer that is actually leaving the visible viewport gets a short
 *    minimum-displacement visibility shot. It moves only enough to recover the
 *    pointer into a safe margin instead of over-panning toward screen centre.
 * 3. Near-edge pointer context has semantic priority over stale COAST intent.
 *    The same SmartCamera keeps its proven gimbal semantics and receives only
 *    an emphasis event when old coast momentum is demonstrably counter-useful.
 *
 * Per-source ArZoom does not opt in and therefore delegates bit-for-bit to the
 * accepted P1 SmartCamera. There is no second follow engine, scene mutation,
 * frame readback, or alternate scene render graph.
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

        if (visibility_shot_active_)
            return step_visibility_shot(input, dt);

        if (sync_active_) {
            synchronize_camera(input);
            if (!camera_close_to_public())
                return public_output_;
            sync_active_ = false;
        }

        if (should_begin_visibility_shot(input, camera_.output())) {
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

    static Vec2 presentation_anchor(Vec2 anchor)
    {
        return {
            std::clamp(anchor.x, 0.18f, 0.82f),
            std::clamp(anchor.y, 0.18f, 0.82f),
        };
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
            /* Zoom +/- is a deliberate presenter command: the pointer becomes
             * the semantic anchor for this temporary affine shot. */
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

        const Vec2 pointer_output = cursor_output_position(
            input.cursor, before.center, before.zoom);
        return !inside_output_margin(pointer_output, 0.055f);
    }

    void begin_visibility_shot(const CameraInput &input,
                               const CameraProfile &profile)
    {
        const float zoom = std::max(public_output_.zoom, 1.0f);
        const Vec2 target_center = minimum_visibility_center(
            input.cursor, public_output_.center, zoom, 0.20f);
        const Vec2 target_pointer_output = cursor_output_position(
            input.cursor, target_center, zoom);
        const float travel_output =
            length(sub(target_center, public_output_.center)) * zoom;

        shot_start_ = screen_transform(public_output_.center, zoom);
        shot_target_ = screen_transform(target_center, zoom);
        shot_elapsed_ = 0.0f;
        shot_duration_ = std::clamp(
            0.32f + 0.15f * travel_output,
            std::max(0.34f, profile.camera_filter_seconds * 0.95f), 0.46f);

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
};

} // namespace arzoom

/* arzoom-filter-v2.cpp historically names its field `arzoom::SmartCamera`.
 * Keep the source ABI/layout change localized: after this header is included,
 * that token resolves to the coordinator above. The legacy SmartCamera remains
 * the single semantic engine owned inside PresenterAwareSmartCamera. */
#define SmartCamera PresenterAwareSmartCamera
