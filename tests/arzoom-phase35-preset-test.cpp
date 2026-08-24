#include "../src/arzoom-cursor-presets.hpp"
#include "../src/arzoom-presentation-cursor.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
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

void preset_metadata_is_safe_and_unique()
{
    std::set<std::string> ids;
    for (const auto &preset : arzoom::kCursorPresets) {
        require(arzoom::cursor_preset_is_valid(preset),
                std::string("invalid built-in cursor preset: ") + preset.id);
        require(ids.insert(preset.id).second,
                std::string("duplicate built-in cursor id: ") + preset.id);
        require(arzoom::find_cursor_preset(preset.id) == &preset,
                std::string("preset lookup mismatch: ") + preset.id);
    }
    require(ids.size() == arzoom::kCursorPresets.size(),
            "preset id count changed unexpectedly");
}

void style_helpers_are_unambiguous()
{
    require(arzoom::cursor_style_is_off("off"), "off style not recognized");
    require(arzoom::cursor_style_is_off(nullptr), "null style should be off");
    require(arzoom::cursor_style_is_custom("custom"),
            "custom style not recognized");
    require(!arzoom::cursor_style_is_custom("prism"),
            "built-in style mistaken for custom");
    require(arzoom::find_cursor_preset("unknown") == nullptr,
            "unknown preset unexpectedly resolved");
}

void preset_hotspots_stay_exact_across_zoom()
{
    for (const auto &preset : arzoom::kCursorPresets) {
        for (float zoom : {1.0f, 2.0f, 3.0f, 4.0f}) {
            const arzoom::Vec2 content{0.73f, 0.41f};
            const arzoom::Vec2 center{0.58f, 0.46f};
            const arzoom::Vec2 hotspot{preset.hotspot_x, preset.hotspot_y};
            const auto geometry = arzoom::presentation_cursor_geometry(
                content, center, zoom,
                {1920.0f, 1080.0f},
                {160.0f, 160.0f},
                hotspot, preset.recommended_size_px);
            const auto reconstructed =
                arzoom::presentation_cursor_hotspot_from_geometry(
                    geometry, hotspot);
            const auto expected = arzoom::project_content_to_output(
                content, center, zoom);
            require(near(reconstructed.x, expected.x) &&
                        near(reconstructed.y, expected.y),
                    std::string("preset hotspot drifted: ") + preset.id);
        }
    }
}

void shipped_playback_contract_is_play_once()
{
    for (const auto &preset : arzoom::kCursorPresets) {
        arzoom::PresentationCursorPlayback playback;
        playback.configure(preset.frame_count, preset.play_seconds);
        require(playback.frame_index() == 0 && !playback.playing(),
                "preset did not start at idle frame 0");
        playback.trigger();
        require(playback.frame_index() == 1 && playback.playing(),
                "preset click did not start at frame 1");
        for (int i = 0; i < 400; ++i)
            playback.advance(1.0f / 240.0f);
        require(playback.frame_index() == 0 && !playback.playing(),
                "preset did not return to idle after one pass");
    }
}

} // namespace

int main()
{
    preset_metadata_is_safe_and_unique();
    style_helpers_are_unambiguous();
    preset_hotspots_stay_exact_across_zoom();
    shipped_playback_contract_is_play_once();
    std::cout << "ArZoom Phase 3.5 built-in cursor preset gates: PASS\n";
    return 0;
}
