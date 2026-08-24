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
        require(preset.play_seconds <= 0.30f,
                std::string("built-in cursor is too slow for tactile feedback: ") +
                    preset.id);
    }
    require(ids.size() == arzoom::kCursorPresets.size(),
            "preset id count changed unexpectedly");
    require(arzoom::kCursorPresets.size() == 7,
            "public built-in cursor palette should contain seven presets");
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

    require(arzoom::find_cursor_preset("parakeet") != nullptr,
            "Parakeet preset missing");
    require(arzoom::find_cursor_preset("classic_hand") != nullptr,
            "Classic Hand preset missing");
    require(arzoom::find_cursor_preset("sticker_hand") != nullptr,
            "Sticker Hand preset missing");
}

void tactile_curve_has_press_rebound_and_settle()
{
    require(near(arzoom::cursor_tactile_press_curve(0.0f), 0.0f),
            "tactile curve does not begin neutral");
    require(arzoom::cursor_tactile_press_curve(0.18f) > 0.98f,
            "tactile curve does not reach a fast press peak");
    require(arzoom::cursor_tactile_press_curve(0.42f) < -0.15f,
            "tactile curve does not overshoot neutral on release");
    require(arzoom::cursor_tactile_press_curve(0.68f) > 0.05f,
            "tactile curve is missing its small counter-bounce");
    require(near(arzoom::cursor_tactile_press_curve(1.0f), 0.0f),
            "tactile curve does not settle exactly to neutral");
}

void hand_presets_use_fingertip_hotspots()
{
    const auto *classic = arzoom::find_cursor_preset("classic_hand");
    const auto *sticker = arzoom::find_cursor_preset("sticker_hand");
    require(classic && sticker, "hand presets unavailable for hotspot gate");

    for (const auto *preset : {classic, sticker}) {
        require(preset->hotspot_x > 0.35f && preset->hotspot_x < 0.48f,
                std::string("hand hotspot X is not on index fingertip: ") +
                    preset->id);
        require(preset->hotspot_y >= 0.0f && preset->hotspot_y < 0.10f,
                std::string("hand hotspot Y is not near fingertip: ") +
                    preset->id);
        require(preset->recommended_size_px >= 56.0f,
                std::string("hand cursor default is too small: ") + preset->id);
    }
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

        bool saw_press_region = false;
        bool saw_late_region = false;
        for (int i = 0; i < 100; ++i) {
            playback.advance(1.0f / 240.0f);
            saw_press_region = saw_press_region ||
                               (playback.playing() && playback.frame_index() >= 4);
            saw_late_region = saw_late_region ||
                              (playback.playing() &&
                               playback.frame_index() > preset.frame_count / 2);
        }
        require(saw_press_region && saw_late_region,
                "tactile preset did not progress through its gesture");
        require(playback.frame_index() == 0 && !playback.playing(),
                "preset did not return to idle after one pass");
    }
}

} // namespace

int main()
{
    preset_metadata_is_safe_and_unique();
    style_helpers_are_unambiguous();
    tactile_curve_has_press_rebound_and_settle();
    hand_presets_use_fingertip_hotspots();
    preset_hotspots_stay_exact_across_zoom();
    shipped_playback_contract_is_play_once();
    std::cout << "ArZoom Phase 3.5 built-in cursor preset gates: PASS\n";
    return 0;
}
