#pragma once

#include "../src/arzoom-math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace arzoom::phase0 {

enum class ClickType { None, Left, Right, Middle, X1, X2 };

struct DesktopRect {
    long left = 0;
    long top = 0;
    long right = 1;
    long bottom = 1;

    long width() const { return right - left; }
    long height() const { return bottom - top; }
    bool valid() const { return width() > 0 && height() > 0; }
};

inline Vec2 normalize_desktop_cursor(long x, long y, const DesktopRect &rect)
{
    if (!rect.valid())
        return {0.5f, 0.5f};
    return {
        static_cast<float>(x - rect.left) / static_cast<float>(rect.width()),
        static_cast<float>(y - rect.top) / static_cast<float>(rect.height()),
    };
}

struct TraceSample {
    float dt = 1.0f / 60.0f;
    Vec2 cursor{0.5f, 0.5f};
    bool zoom_requested = true;
    float configured_zoom = 2.0f;
    ClickType click = ClickType::None;
};

struct CameraSnapshot {
    Vec2 center{0.5f, 0.5f};
    float zoom = 1.0f;
    Vec2 cursor_output{0.5f, 0.5f};
};

struct TraceMetrics {
    float max_center_displacement = 0.0f;
    float max_apparent_speed = 0.0f;
    float max_edge_violation = 0.0f;
    float max_cursor_output_distance = 0.0f;
    std::size_t camera_move_frames = 0;
    std::size_t click_count = 0;
    CameraSnapshot final{};
};

class BaselineCamera {
public:
    CameraSnapshot snapshot(Vec2 cursor) const
    {
        return {center_, zoom_, cursor_output_position(cursor, center_, std::max(zoom_, 1.0f))};
    }

    CameraSnapshot step(const TraceSample &sample)
    {
        const float dt = std::clamp(sample.dt, 0.0f, 0.10f);
        const float target_zoom = sample.zoom_requested ? std::clamp(sample.configured_zoom, 1.10f, 4.00f) : 1.0f;

        if (!sample.zoom_requested) {
            target_center_ = {0.5f, 0.5f};
        } else {
            target_center_ = smart_follow_target(sample.cursor, center_, anchor_, safe_zone_, std::max(sample.configured_zoom, 1.01f));
        }

        if (target_zoom >= zoom_) {
            zoom_ = smooth_scalar(zoom_, target_zoom, dt, zoom_in_seconds_);
            target_center_ = clamp_center(target_center_, std::max(zoom_, 1.0f));
            center_ = smooth_center(center_, target_center_, dt, pan_seconds_, max_output_speed_, std::max(zoom_, 1.0f));
        } else {
            target_center_ = clamp_center(target_center_, std::max(target_zoom, 1.0f));
            center_ = smooth_center(center_, target_center_, dt, pan_seconds_, max_output_speed_, std::max(zoom_, 1.0f));
            const float next_zoom = smooth_scalar(zoom_, target_zoom, dt, zoom_out_seconds_);
            zoom_ = std::max(next_zoom, minimum_zoom_for_center(center_));
            center_ = clamp_center(center_, std::max(zoom_, 1.0f));
        }

        if (!sample.zoom_requested && nearly_equal(zoom_, 1.0f, 0.001f) && nearly_equal(center_, {0.5f, 0.5f}, 0.001f)) {
            zoom_ = 1.0f;
            center_ = {0.5f, 0.5f};
            target_center_ = {0.5f, 0.5f};
        }

        return snapshot(sample.cursor);
    }

private:
    Vec2 center_{0.5f, 0.5f};
    Vec2 target_center_{0.5f, 0.5f};
    float zoom_ = 1.0f;
    Vec2 anchor_{0.5f, 0.45f};
    float safe_zone_ = 0.28f;
    float zoom_in_seconds_ = 0.34f;
    float zoom_out_seconds_ = 0.30f;
    float pan_seconds_ = 0.23f;
    float max_output_speed_ = 1.35f;
};

inline float edge_violation(const CameraSnapshot &s)
{
    const float half = 0.5f / std::max(s.zoom, 1.0f);
    float violation = 0.0f;
    violation = std::max(violation, -(s.center.x - half));
    violation = std::max(violation, -(s.center.y - half));
    violation = std::max(violation, s.center.x + half - 1.0f);
    violation = std::max(violation, s.center.y + half - 1.0f);
    return std::max(0.0f, violation);
}

inline TraceMetrics replay(const std::vector<TraceSample> &trace)
{
    BaselineCamera camera;
    TraceMetrics metrics;
    Vec2 initial_center{0.5f, 0.5f};
    Vec2 previous_center = initial_center;

    for (const auto &sample : trace) {
        const CameraSnapshot snap = camera.step(sample);
        const Vec2 delta = sub(snap.center, previous_center);
        const float center_step = length(delta);
        const float dt = std::max(sample.dt, 1.0e-6f);
        const float apparent_speed = center_step * std::max(snap.zoom, 1.0f) / dt;

        metrics.max_center_displacement = std::max(metrics.max_center_displacement, length(sub(snap.center, initial_center)));
        metrics.max_apparent_speed = std::max(metrics.max_apparent_speed, apparent_speed);
        metrics.max_edge_violation = std::max(metrics.max_edge_violation, edge_violation(snap));
        metrics.max_cursor_output_distance = std::max(metrics.max_cursor_output_distance, length(sub(snap.cursor_output, Vec2{0.5f, 0.5f})));
        if (center_step > 1.0e-6f)
            ++metrics.camera_move_frames;
        if (sample.click != ClickType::None)
            ++metrics.click_count;

        previous_center = snap.center;
        metrics.final = snap;
    }

    return metrics;
}

inline void append_hold(std::vector<TraceSample> &trace, Vec2 cursor, float seconds, int fps, bool zoomed = true, float zoom = 2.0f)
{
    const int frames = std::max(1, static_cast<int>(std::lround(seconds * static_cast<float>(fps))));
    for (int i = 0; i < frames; ++i)
        trace.push_back({1.0f / static_cast<float>(fps), cursor, zoomed, zoom, ClickType::None});
}

inline std::vector<TraceSample> warm_zoom(int fps = 60)
{
    std::vector<TraceSample> trace;
    append_hold(trace, {0.5f, 0.5f}, 1.0f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> jitter_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const int frames = 2 * fps;
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i);
        const Vec2 p{0.5f + 0.015f * std::sin(t * 0.80f), 0.5f + 0.015f * std::cos(t * 1.10f)};
        trace.push_back({1.0f / fps, p, true, 2.0f, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> explanation_circle_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const int frames = 2 * fps;
    constexpr float two_pi = 6.2831853071795864769f;
    for (int i = 0; i < frames; ++i) {
        const float phase = two_pi * static_cast<float>(i) / static_cast<float>(frames);
        const Vec2 p{0.5f + 0.05f * std::cos(phase), 0.5f + 0.05f * std::sin(phase)};
        trace.push_back({1.0f / fps, p, true, 2.0f, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> repeated_pointing_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const Vec2 points[] = {{0.46f, 0.48f}, {0.54f, 0.49f}, {0.47f, 0.52f}, {0.55f, 0.51f}};
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (const Vec2 p : points)
            append_hold(trace, p, 0.12f, fps, true, 2.0f);
    }
    return trace;
}

inline std::vector<TraceSample> nearby_deliberate_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const int frames = fps;
    for (int i = 0; i < frames; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const Vec2 p{0.50f + 0.18f * u, 0.50f - 0.08f * u};
        trace.push_back({1.0f / fps, p, true, 2.0f, ClickType::None});
    }
    append_hold(trace, {0.68f, 0.42f}, 0.4f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> negative_monitor_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const DesktopRect monitor{-1920, -180, 0, 900};
    const int frames = fps;
    for (int i = 0; i < frames; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const long x = static_cast<long>(std::lround(-1800.0 + 1650.0 * u));
        const long y = static_cast<long>(std::lround(-80.0 + 850.0 * u));
        trace.push_back({1.0f / fps, normalize_desktop_cursor(x, y, monitor), true, 2.0f, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> mixed_aspect_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    // 3440x1440 desktop coordinates are normalized before entering camera math.
    // The normalized camera path therefore remains valid when the OBS output/canvas
    // uses a different aspect ratio, such as 1920x1080.
    const DesktopRect ultrawide{0, 0, 3440, 1440};
    const int frames = 2 * fps;
    for (int i = 0; i < frames; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const long x = static_cast<long>(std::lround(120.0 + 3160.0 * u));
        const long y = static_cast<long>(std::lround(720.0 + 520.0 * std::sin(u * 6.28318530718f)));
        trace.push_back({1.0f / fps, normalize_desktop_cursor(x, y, ultrawide), true, 2.0f, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> long_relocation_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    append_hold(trace, {0.90f, 0.50f}, 1.0f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> direction_reversal_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    append_hold(trace, {0.90f, 0.50f}, 0.45f, fps, true, 2.0f);
    append_hold(trace, {0.10f, 0.50f}, 0.90f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> edge_travel_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const int frames = 2 * fps;
    for (int i = 0; i < frames; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const Vec2 p{0.98f, 0.10f + 0.80f * u};
        trace.push_back({1.0f / fps, p, true, 2.0f, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> corner_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    append_hold(trace, {0.98f, 0.98f}, 1.2f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> click_after_relocation_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    append_hold(trace, {0.88f, 0.26f}, 0.45f, fps, true, 2.0f);
    trace.push_back({1.0f / fps, {0.88f, 0.26f}, true, 2.0f, ClickType::Left});
    append_hold(trace, {0.88f, 0.26f}, 0.30f, fps, true, 2.0f);
    return trace;
}

inline std::vector<TraceSample> rapid_click_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    const ClickType clicks[] = {ClickType::Left, ClickType::Right, ClickType::Left, ClickType::Middle, ClickType::Right};
    for (ClickType click : clicks) {
        trace.push_back({1.0f / fps, {0.60f, 0.54f}, true, 2.0f, click});
        append_hold(trace, {0.60f, 0.54f}, 0.08f, fps, true, 2.0f);
    }
    return trace;
}

inline std::vector<TraceSample> zoom_while_moving_trace(int fps = 60)
{
    std::vector<TraceSample> trace;
    const int frames = 2 * fps;
    for (int i = 0; i < frames; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const Vec2 p{0.30f + 0.55f * u, 0.35f + 0.20f * u};
        trace.push_back({1.0f / fps, p, true, 1.25f + 1.75f * u, ClickType::None});
    }
    return trace;
}

inline std::vector<TraceSample> zoom_out_near_edge_trace(int fps = 60)
{
    auto trace = warm_zoom(fps);
    append_hold(trace, {0.96f, 0.50f}, 0.8f, fps, true, 3.0f);
    append_hold(trace, {0.96f, 0.50f}, 1.2f, fps, false, 3.0f);
    return trace;
}

} // namespace arzoom::phase0
