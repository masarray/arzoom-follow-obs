#pragma once

#include "arzoom-click-visual.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace arzoom {

/*
 * Presentation Cursor core
 * ------------------------
 * Runtime behavior is intentionally tiny and platform-neutral:
 *
 *   idle frame 0 -> click restarts a single pass -> frame 0
 *
 * Image decoding / GPU upload is deliberately outside this class. The video
 * hot path owns only fixed scalar state and an integer frame index.
 */
class PresentationCursorPlayback {
public:
    void configure(size_t frame_count, float play_seconds = 0.46f)
    {
        frame_count_ = std::max<size_t>(1, frame_count);
        play_seconds_ = std::clamp(play_seconds, 0.08f, 2.0f);
        reset();
    }

    void reset()
    {
        playing_ = false;
        elapsed_ = 0.0f;
        frame_index_ = 0;
    }

    void trigger()
    {
        elapsed_ = 0.0f;
        if (frame_count_ <= 1) {
            playing_ = false;
            frame_index_ = 0;
            return;
        }

        /* Frame 0 is the permanent idle pose. Start at frame 1 immediately so
         * a click never waits through an asset's baked-in idle delay. */
        playing_ = true;
        frame_index_ = 1;
    }

    void advance(float dt)
    {
        if (!playing_ || frame_count_ <= 1)
            return;

        elapsed_ += std::clamp(dt, 0.0f, 0.10f);
        if (elapsed_ >= play_seconds_) {
            reset();
            return;
        }

        const size_t animated_frames = frame_count_ - 1;
        const float progress = std::clamp(elapsed_ / play_seconds_, 0.0f, 1.0f);
        const size_t offset = std::min(
            animated_frames - 1,
            static_cast<size_t>(std::floor(progress * animated_frames)));
        frame_index_ = 1 + offset;
    }

    size_t frame_index() const { return frame_index_; }
    size_t frame_count() const { return frame_count_; }
    bool playing() const { return playing_; }
    float elapsed_seconds() const { return elapsed_; }

private:
    size_t frame_count_ = 1;
    size_t frame_index_ = 0;
    float play_seconds_ = 0.46f;
    float elapsed_ = 0.0f;
    bool playing_ = false;
};

struct PresentationCursorGeometry {
    Vec2 hotspot_output{0.5f, 0.5f};
    Vec2 top_left_output{0.5f, 0.5f};
    Vec2 size_output{0.0f, 0.0f};
};

/*
 * Presentation Cursor is composited after the camera transform, so its visual
 * size remains constant in output pixels even at 2x/3x/4x. Its hotspot still
 * follows the same content->output transform as the accepted click rings.
 */
inline PresentationCursorGeometry presentation_cursor_geometry(
    Vec2 content_position,
    Vec2 camera_center,
    float zoom,
    Vec2 viewport_pixels,
    Vec2 asset_pixels,
    Vec2 hotspot_normalized,
    float cursor_height_pixels)
{
    PresentationCursorGeometry geometry;
    geometry.hotspot_output = project_content_to_output(
        content_position, camera_center, zoom);

    const float viewport_width = std::max(viewport_pixels.x, 1.0f);
    const float viewport_height = std::max(viewport_pixels.y, 1.0f);
    const float asset_width = std::max(asset_pixels.x, 1.0f);
    const float asset_height = std::max(asset_pixels.y, 1.0f);
    const float safe_height = std::clamp(cursor_height_pixels, 8.0f, 256.0f);
    const float display_width = safe_height * (asset_width / asset_height);

    geometry.size_output = {
        display_width / viewport_width,
        safe_height / viewport_height,
    };

    hotspot_normalized.x = std::clamp(hotspot_normalized.x, 0.0f, 1.0f);
    hotspot_normalized.y = std::clamp(hotspot_normalized.y, 0.0f, 1.0f);
    geometry.top_left_output = {
        geometry.hotspot_output.x -
            hotspot_normalized.x * geometry.size_output.x,
        geometry.hotspot_output.y -
            hotspot_normalized.y * geometry.size_output.y,
    };
    return geometry;
}

inline Vec2 presentation_cursor_hotspot_from_geometry(
    const PresentationCursorGeometry &geometry,
    Vec2 hotspot_normalized)
{
    hotspot_normalized.x = std::clamp(hotspot_normalized.x, 0.0f, 1.0f);
    hotspot_normalized.y = std::clamp(hotspot_normalized.y, 0.0f, 1.0f);
    return {
        geometry.top_left_output.x +
            hotspot_normalized.x * geometry.size_output.x,
        geometry.top_left_output.y +
            hotspot_normalized.y * geometry.size_output.y,
    };
}

} // namespace arzoom
