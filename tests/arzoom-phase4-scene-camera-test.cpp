#include "../src/arzoom-scene-camera-core.hpp"

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

void managed_filter_identity_is_exact()
{
    require(arzoom::scene_camera_filter_matches(
                "arzoom_filter", "ArZoom Camera"),
            "managed scene-camera filter identity was not recognized");
    require(!arzoom::scene_camera_filter_matches(
                "arzoom_filter", "ArZoom - Smart Camera Zoom & Follow"),
            "ordinary per-source ArZoom filter was mistaken for Scene Camera");
    require(!arzoom::scene_camera_filter_matches(
                "other_filter", "ArZoom Camera"),
            "foreign filter with matching name was mistaken for Scene Camera");
}

void toggle_policy_is_deterministic()
{
    using arzoom::SceneCameraToggleAction;
    require(arzoom::scene_camera_toggle_action(false, false) ==
                SceneCameraToggleAction::CreateEnabled,
            "absent Scene Camera did not request create+enable");
    require(arzoom::scene_camera_toggle_action(true, false) ==
                SceneCameraToggleAction::EnableExisting,
            "disabled Scene Camera did not request enable");
    require(arzoom::scene_camera_toggle_action(true, true) ==
                SceneCameraToggleAction::DisableExisting,
            "enabled Scene Camera did not request disable");
}

void fullscreen_mapping_contract_rejects_guesses()
{
    using arzoom::SceneMappingQuad;

    const SceneMappingQuad fullscreen{
        {0.0f, 0.0f}, {1920.0f, 0.0f},
        {0.0f, 1080.0f}, {1920.0f, 1080.0f}};
    require(arzoom::scene_mapping_is_full_canvas(fullscreen, 1920.0f, 1080.0f),
            "exact fullscreen mapping was rejected");

    SceneMappingQuad tiny_numeric_noise = fullscreen;
    tiny_numeric_noise.top_left = {0.4f, -0.3f};
    tiny_numeric_noise.bottom_right = {1919.5f, 1080.4f};
    require(arzoom::scene_mapping_is_full_canvas(
                tiny_numeric_noise, 1920.0f, 1080.0f),
            "small transform rounding noise broke fullscreen mapping");

    const SceneMappingQuad inset{
        {120.0f, 80.0f}, {1800.0f, 80.0f},
        {120.0f, 1000.0f}, {1800.0f, 1000.0f}};
    require(!arzoom::scene_mapping_is_full_canvas(inset, 1920.0f, 1080.0f),
            "scaled/inset Display Capture was incorrectly treated as fullscreen");

    const SceneMappingQuad rotated_like{
        {0.0f, 0.0f}, {1900.0f, 120.0f},
        {-80.0f, 1060.0f}, {1820.0f, 1180.0f}};
    require(!arzoom::scene_mapping_is_full_canvas(
                rotated_like, 1920.0f, 1080.0f),
            "rotated mapping was incorrectly accepted");

    require(!arzoom::scene_mapping_is_full_canvas(fullscreen, 0.0f, 1080.0f),
            "zero scene width was accepted");
    require(!arzoom::scene_mapping_is_full_canvas(fullscreen, 1920.0f, 0.0f),
            "zero scene height was accepted");
}

} // namespace

int main()
{
    managed_filter_identity_is_exact();
    toggle_policy_is_deterministic();
    fullscreen_mapping_contract_rejects_guesses();
    std::cout << "ArZoom Phase 4 native Scene Camera gates: PASS\n";
    return 0;
}
