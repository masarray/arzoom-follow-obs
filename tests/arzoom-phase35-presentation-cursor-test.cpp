#include "../src/arzoom-presentation-cursor.hpp"
#include "../src/arzoom-presenter-controls.hpp"
#include "../src/arzoom-smart-zone-camera.hpp"

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

bool near(float a, float b, float epsilon = 1.0e-5f)
{
    return std::fabs(a - b) <= epsilon;
}

bool near(arzoom::Vec2 a, arzoom::Vec2 b, float epsilon = 1.0e-5f)
{
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

void idle_frame_is_stable()
{
    arzoom::PresentationCursorPlayback playback;
    playback.configure(25, 0.46f);
    for (int i = 0; i < 1000; ++i)
        playback.advance(1.0f / 144.0f);
    require(playback.frame_index() == 0,
            "idle Presentation Cursor left frame 0");
    require(!playback.playing(),
            "idle Presentation Cursor unexpectedly started playback");
}

void click_plays_once_and_returns_idle()
{
    arzoom::PresentationCursorPlayback playback;
    playback.configure(25, 0.46f);
    playback.trigger();
    require(playback.playing() && playback.frame_index() == 1,
            "click did not start immediately at frame 1");

    bool saw_late_frame = false;
    for (int i = 0; i < 120; ++i) {
        playback.advance(1.0f / 120.0f);
        saw_late_frame = saw_late_frame || playback.frame_index() > 12;
    }
    require(saw_late_frame,
            "play-once cursor never progressed through the animation");
    require(!playback.playing() && playback.frame_index() == 0,
            "play-once cursor did not return to idle frame 0");
}

void click_during_playback_restarts()
{
    arzoom::PresentationCursorPlayback playback;
    playback.configure(25, 0.46f);
    playback.trigger();
    for (int i = 0; i < 20; ++i)
        playback.advance(1.0f / 120.0f);
    require(playback.frame_index() > 1,
            "restart test did not advance before second click");

    playback.trigger();
    require(playback.frame_index() == 1 &&
                near(playback.elapsed_seconds(), 0.0f, 1.0e-7f),
            "click during playback did not restart from frame 1");
}

void hotspot_tracks_click_center_across_zoom()
{
    using namespace arzoom;
    const float zooms[] = {1.0f, 2.0f, 3.0f, 4.0f};
    const Vec2 contents[] = {
        {0.02f, 0.03f}, {0.35f, 0.72f}, {0.98f, 0.96f}};
    const Vec2 center{0.58f, 0.41f};
    const Vec2 hotspot{0.18f, 0.18f};

    for (float zoom : zooms) {
        for (Vec2 content : contents) {
            const PresentationCursorGeometry geometry =
                presentation_cursor_geometry(
                    content, center, zoom,
                    {1920.0f, 1080.0f}, {192.0f, 192.0f},
                    hotspot, 52.0f);
            const Vec2 reconstructed =
                presentation_cursor_hotspot_from_geometry(
                    geometry, hotspot);
            const Vec2 click_center = project_content_to_output(
                content, center, zoom);
            require(near(reconstructed, click_center, 2.0e-6f),
                    "cursor hotspot drifted away from click center");
        }
    }
}

void cursor_size_is_output_pixel_constant()
{
    using namespace arzoom;
    const Vec2 content{0.63f, 0.48f};
    const Vec2 hotspot{0.18f, 0.18f};
    const PresentationCursorGeometry one = presentation_cursor_geometry(
        content, {0.5f, 0.5f}, 1.0f,
        {1920.0f, 1080.0f}, {192.0f, 192.0f}, hotspot, 52.0f);
    const PresentationCursorGeometry four = presentation_cursor_geometry(
        content, {0.5f, 0.5f}, 4.0f,
        {1920.0f, 1080.0f}, {192.0f, 192.0f}, hotspot, 52.0f);
    require(near(one.size_output, four.size_output, 1.0e-8f),
            "Presentation Cursor size changed with camera zoom");
}

void overview_mapping_keeps_hotspot_exact()
{
    using namespace arzoom;
    OverviewPeekController overview;
    require(overview.begin({0.70f, 0.34f}, 2.8f),
            "overview cursor test could not start");

    const Vec2 content{0.61f, 0.43f};
    const Vec2 hotspot{0.18f, 0.18f};
    for (int frame = 0; frame < 120; ++frame) {
        const OverviewOutput output =
            overview.step(1.0f / 120.0f, 0.34f, 0.32f);
        const auto geometry = presentation_cursor_geometry(
            content, output.center, output.zoom,
            {2560.0f, 1440.0f}, {192.0f, 192.0f}, hotspot, 56.0f);
        require(near(
                    presentation_cursor_hotspot_from_geometry(
                        geometry, hotspot),
                    project_content_to_output(
                        content, output.center, output.zoom),
                    2.0e-6f),
                "cursor hotspot lost alignment during Overview Peek");
        if (output.phase == OverviewPhase::Holding)
            break;
    }
}

void cursor_state_is_camera_isolated()
{
    using namespace arzoom;
    SmartCamera baseline;
    SmartCamera with_cursor;
    PresentationCursorPlayback playback;
    playback.configure(25, 0.46f);

    for (int frame = 0; frame < 600; ++frame) {
        CameraInput input;
        input.dt = 1.0f / 120.0f;
        input.zoom_requested = frame >= 20;
        input.configured_zoom = 2.5f;
        input.cursor_valid = true;
        input.cursor = frame < 250 ? Vec2{0.30f, 0.45f}
                                   : Vec2{0.78f, 0.62f};
        input.follow_policy = CameraFollowPolicy::Smart;
        input.motion_style = CameraMotionStyle::Balanced;

        if (frame == 100 || frame == 280 || frame == 400)
            playback.trigger();
        playback.advance(input.dt);

        const CameraOutput a = baseline.step(input);
        const CameraOutput b = with_cursor.step(input);
        require(near(a.center, b.center, 1.0e-7f) &&
                    near(a.zoom, b.zoom, 1.0e-7f) &&
                    a.state == b.state &&
                    near(a.urgency, b.urgency, 1.0e-7f),
                "Presentation Cursor state changed Smart Camera output");
    }
}

} // namespace

int main()
{
    idle_frame_is_stable();
    click_plays_once_and_returns_idle();
    click_during_playback_restarts();
    hotspot_tracks_click_center_across_zoom();
    cursor_size_is_output_pixel_constant();
    overview_mapping_keeps_hotspot_exact();
    cursor_state_is_camera_isolated();
    std::cout << "ArZoom Phase 3.5 Presentation Cursor gates: PASS\n";
    return 0;
}
