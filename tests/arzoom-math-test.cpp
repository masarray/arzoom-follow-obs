#include "../src/arzoom-math.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

using arzoom::Vec2;

int main()
{
    {
        const Vec2 center = arzoom::clamp_center({0.0f, 1.0f}, 2.0f);
        assert(arzoom::nearly_equal(center.x, 0.25f));
        assert(arzoom::nearly_equal(center.y, 0.75f));
    }

    {
        const Vec2 current{0.5f, 0.5f};
        const Vec2 cursor{0.52f, 0.47f};
        const Vec2 target = arzoom::smart_follow_target(
            cursor, current, {0.5f, 0.45f}, 0.28f, 2.0f);
        assert(arzoom::nearly_equal(target, current));
    }

    {
        const Vec2 current{0.5f, 0.5f};
        const Vec2 cursor{0.95f, 0.5f};
        const Vec2 target = arzoom::smart_follow_target(
            cursor, current, {0.5f, 0.45f}, 0.28f, 2.0f);
        assert(target.x > current.x);
        assert(target.x <= 0.75f);
    }

    {
        float value = 1.0f;
        for (int i = 0; i < 60; ++i)
            value = arzoom::smooth_scalar(value, 2.0f, 1.0f / 60.0f, 0.34f);
        assert(value > 1.999f);
    }

    /*
     * Randomized edge invariant: a valid center always maps the output corners
     * to valid input UV coordinates. This is the no-black-edge contract.
     */
    std::mt19937 rng(61850);
    std::uniform_real_distribution<float> zoom_dist(1.0f, 4.0f);
    std::uniform_real_distribution<float> center_dist(-1.0f, 2.0f);

    for (int i = 0; i < 200000; ++i) {
        const float zoom = zoom_dist(rng);
        const Vec2 center = arzoom::clamp_center(
            {center_dist(rng), center_dist(rng)}, zoom);

        const float half = 0.5f / zoom;
        const float left = center.x - half;
        const float right = center.x + half;
        const float top = center.y - half;
        const float bottom = center.y + half;

        assert(left >= -1.0e-6f);
        assert(top >= -1.0e-6f);
        assert(right <= 1.0f + 1.0e-6f);
        assert(bottom <= 1.0f + 1.0e-6f);
    }


    /*
     * Long dynamic simulation: random cursor motion, frequent toggle reversals,
     * and changing zoom amounts must never expose an invalid edge. Pan motion
     * also stays inside its configured apparent screen-speed limit.
     */
    {
        Vec2 current_center{0.5f, 0.5f};
        Vec2 target_center{0.5f, 0.5f};
        Vec2 cursor{0.5f, 0.5f};
        float current_zoom = 1.0f;
        float target_zoom = 1.0f;
        bool zoomed = false;
        std::uniform_real_distribution<float> cursor_step(-0.035f, 0.035f);
        std::uniform_real_distribution<float> target_zoom_dist(1.20f, 4.0f);

        for (int frame = 0; frame < 120000; ++frame) {
            const float dt = (frame % 97 == 0) ? 1.0f / 30.0f : 1.0f / 60.0f;

            if (frame % 911 == 0) {
                zoomed = !zoomed;
                target_zoom = zoomed ? target_zoom_dist(rng) : 1.0f;
            }
            if (zoomed && frame % 1733 == 0)
                target_zoom = target_zoom_dist(rng);

            cursor.x = std::clamp(cursor.x + cursor_step(rng), 0.0f, 1.0f);
            cursor.y = std::clamp(cursor.y + cursor_step(rng), 0.0f, 1.0f);

            if (zoomed) {
                target_center = arzoom::smart_follow_target(
                    cursor, current_center, {0.5f, 0.45f}, 0.28f,
                    std::max(target_zoom, 1.01f));
            } else {
                target_center = {0.5f, 0.5f};
            }

            const Vec2 before = current_center;
            if (target_zoom >= current_zoom) {
                current_zoom = arzoom::smooth_scalar(
                    current_zoom, target_zoom, dt, 0.34f);
                target_center = arzoom::clamp_center(
                    target_center, std::max(current_zoom, 1.0f));
                current_center = arzoom::smooth_center(
                    current_center, target_center, dt, 0.23f, 1.35f,
                    std::max(current_zoom, 1.0f));
            } else {
                target_center = arzoom::clamp_center(
                    target_center, std::max(target_zoom, 1.0f));
                current_center = arzoom::smooth_center(
                    current_center, target_center, dt, 0.23f, 1.35f,
                    std::max(current_zoom, 1.0f));
                const float next_zoom = arzoom::smooth_scalar(
                    current_zoom, target_zoom, dt, 0.30f);
                current_zoom = std::max(
                    next_zoom,
                    arzoom::minimum_zoom_for_center(current_center));
                current_center = arzoom::clamp_center(
                    current_center, std::max(current_zoom, 1.0f));
            }

            const float apparent_step =
                arzoom::length(arzoom::sub(current_center, before)) *
                std::max(current_zoom, 1.0f);
            assert(apparent_step <= 1.35f * dt + 1.0e-4f);

            const float half = 0.5f / std::max(current_zoom, 1.0f);
            assert(current_center.x - half >= -1.0e-6f);
            assert(current_center.y - half >= -1.0e-6f);
            assert(current_center.x + half <= 1.0f + 1.0e-6f);
            assert(current_center.y + half <= 1.0f + 1.0e-6f);
        }
    }

    std::cout << "ArZoom motion and edge tests: PASS\n";
    return 0;
}
