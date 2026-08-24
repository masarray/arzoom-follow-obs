#include "../src/arzoom-camera.hpp"
#include "../src/arzoom-click-visual.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

bool near(float a, float b, float epsilon = 1.0e-6f)
{
    return std::fabs(a - b) <= epsilon;
}

bool near(arzoom::Vec2 a, arzoom::Vec2 b, float epsilon = 1.0e-6f)
{
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

arzoom::CameraInput sample(float dt, arzoom::Vec2 cursor,
                           bool zoom_requested = true,
                           float zoom = 2.0f)
{
    arzoom::CameraInput input;
    input.dt = dt;
    input.cursor = cursor;
    input.cursor_valid = true;
    input.zoom_requested = zoom_requested;
    input.configured_zoom = zoom;
    input.follow_policy = arzoom::CameraFollowPolicy::Smart;
    input.motion_style = arzoom::CameraMotionStyle::Balanced;
    return input;
}

void fixed_slots_are_bounded_and_ordered()
{
    using namespace arzoom;
    ClickVisualState state;

    state.push(ClickType::Left, {0.10f, 0.20f});
    state.push(ClickType::Right, {0.20f, 0.30f});
    state.push(ClickType::Middle, {0.30f, 0.40f});
    state.push(ClickType::Left, {0.40f, 0.50f});
    require(state.active_count() == ClickVisualState::kSlotCount,
            "four click slots were not retained");

    uint32_t oldest_generation = 0xffffffffu;
    for (size_t i = 0; i < ClickVisualState::kSlotCount; ++i)
        oldest_generation = std::min(oldest_generation,
                                     state.slot(i).generation);

    state.push(ClickType::Right, {0.90f, 0.80f});
    require(state.active_count() == ClickVisualState::kSlotCount,
            "rapid clicks grew beyond fixed capacity");

    bool found_newest = false;
    bool found_oldest = false;
    for (size_t i = 0; i < ClickVisualState::kSlotCount; ++i) {
        const ClickEvent &event = state.slot(i);
        found_newest = found_newest ||
            (event.type == ClickType::Right &&
             near(event.content_position, {0.90f, 0.80f}));
        found_oldest = found_oldest || event.generation == oldest_generation;
    }
    require(found_newest, "new click did not replace the oldest slot");
    require(!found_oldest, "oldest click slot was not recycled");
}

void premium_click_lifetime_contract()
{
    using namespace arzoom;
    require(near(click_lifetime_seconds(ClickType::Left), 0.44f),
            "left click lifetime drifted from premium dual-ring timing");
    require(near(click_lifetime_seconds(ClickType::Right), 0.50f),
            "right click lifetime drifted from premium dual-ring timing");
    require(near(click_lifetime_seconds(ClickType::Middle), 0.32f),
            "middle click lifetime drifted from compact timing");
    require(click_lifetime_seconds(ClickType::Right) <= 0.50f,
            "click feedback is allowed to linger too long for tutorials");
}

void click_lifetimes_expire_without_residual_state()
{
    using namespace arzoom;
    ClickVisualState state;
    state.push(ClickType::Left, {0.2f, 0.2f});
    state.push(ClickType::Right, {0.4f, 0.4f});
    state.push(ClickType::Middle, {0.6f, 0.6f});

    for (int frame = 0; frame < 40; ++frame)
        state.advance(1.0f / 60.0f);

    require(!state.has_active(),
            "expired click visual left residual active state");
    require(state.active_count() == 0,
            "expired click slots did not clear exactly");
}

void content_anchor_tracks_camera_transform()
{
    using namespace arzoom;
    const Vec2 click{0.82f, 0.28f};

    const struct {
        Vec2 center;
        float zoom;
    } cameras[] = {
        {{0.50f, 0.50f}, 1.0f},
        {{0.62f, 0.44f}, 2.0f},
        {{0.74f, 0.34f}, 3.0f},
        {{0.82f, 0.28f}, 4.0f},
    };

    for (const auto &camera : cameras) {
        const Vec2 projected = project_content_to_output(
            click, camera.center, camera.zoom);
        const Vec2 recovered{
            camera.center.x + (projected.x - 0.5f) / camera.zoom,
            camera.center.y + (projected.y - 0.5f) / camera.zoom,
        };
        require(near(recovered, click, 2.0e-6f),
                "click content anchor drifted under camera transform");
    }
}

void click_subsystem_does_not_retarget_camera()
{
    using namespace arzoom;
    SmartCamera baseline;
    SmartCamera with_clicks;
    ClickVisualState clicks;

    for (int frame = 0; frame < 480; ++frame) {
        Vec2 cursor{0.50f, 0.50f};
        bool zoom_on = frame >= 10;
        if (frame >= 90 && frame < 230)
            cursor = {0.88f, 0.46f};
        else if (frame >= 230 && frame < 330)
            cursor = {0.80f + 0.045f * std::cos(frame * 0.19f),
                      0.48f + 0.045f * std::sin(frame * 0.19f)};
        else if (frame >= 330)
            cursor = {0.20f, 0.70f};

        const CameraInput input = sample(
            1.0f / 60.0f, cursor, zoom_on, 2.0f);
        const CameraOutput a = baseline.step(input);

        clicks.advance(input.dt);
        if (frame == 105 || frame == 238 || frame == 245 || frame == 350) {
            const ClickType type = frame == 350
                                       ? ClickType::Right
                                       : ClickType::Left;
            clicks.push(type, cursor);
        }
        const CameraOutput b = with_clicks.step(input);

        require(near(a.center, b.center, 1.0e-7f),
                "click subsystem changed Smart Zone camera center");
        require(near(a.zoom, b.zoom, 1.0e-7f),
                "click subsystem changed camera zoom");
        require(a.state == b.state,
                "click subsystem changed camera state transition");
        require(near(a.urgency, b.urgency, 1.0e-7f),
                "click subsystem changed camera urgency");
    }
}

void edge_click_coordinates_remain_finite()
{
    using namespace arzoom;
    const Vec2 clicks[] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f},
    };
    const float zooms[] = {2.0f, 3.0f, 4.0f};

    for (float zoom : zooms) {
        const Vec2 center = clamp_center({0.78f, 0.22f}, zoom);
        for (Vec2 click : clicks) {
            const Vec2 projected = project_content_to_output(
                click, center, zoom);
            require(std::isfinite(projected.x) && std::isfinite(projected.y),
                    "edge click projection produced non-finite coordinate");
        }
    }
}

} // namespace

int main()
{
    fixed_slots_are_bounded_and_ordered();
    premium_click_lifetime_contract();
    click_lifetimes_expire_without_residual_state();
    content_anchor_tracks_camera_transform();
    click_subsystem_does_not_retarget_camera();
    edge_click_coordinates_remain_finite();
    std::cout << "ArZoom Phase 2 GPU click gates: PASS\n";
    return 0;
}
