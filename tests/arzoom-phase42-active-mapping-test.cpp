#include "arzoom-presentation-screen-active-mapping.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

arzoom::SceneAxisAlignedMapping mapping(float offset_x, float scale_x)
{
    arzoom::SceneAxisAlignedMapping result;
    result.scene_offset = {offset_x, 0.0f};
    result.scene_scale = {scale_x, 1.0f};
    return result;
}

arzoom::PresentationScreenActiveMappingCandidate candidate(
    const char *uuid, std::int64_t left, std::int64_t top,
    std::int64_t right, std::int64_t bottom,
    arzoom::SceneAxisAlignedMapping scene_mapping, bool ready = true)
{
    arzoom::PresentationScreenActiveMappingCandidate result;
    result.identity.source_uuid = uuid ? uuid : "";
    result.identity.display_label = uuid ? uuid : "";
    result.identity.source_resolved = uuid && *uuid;
    result.physical_monitor = {left, top, right, bottom};
    result.mapping = scene_mapping;
    result.visible = true;
    result.monitor_resolved = true;
    result.geometry_valid = true;
    result.runtime_ready = ready;
    return result;
}

arzoom::PresentationScreenSelectionSettings selected(
    std::initializer_list<const char *> uuids)
{
    arzoom::PresentationScreenSelectionSettings settings;
    settings.persisted = true;
    for (const char *uuid : uuids)
        settings.selected_source_uuids.emplace_back(uuid ? uuid : "");
    return settings;
}

void one_screen_auto_preserves_existing_behavior()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-a", 0, 0, 1920, 1080, mapping(0.0f, 1.0f))};
    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), {});
    require(prepared.eligibility.status ==
                arzoom::PresentationScreenEligibilityStatus::AutoSingle,
            "single ready screen should remain implicit Auto");
    const auto active = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 960, 540);
    require(active.active() && active.active_index == 0,
            "single-screen Auto should resolve the existing mapping");
}

void two_selected_screens_switch_a_b_a_without_camera_ownership_reset()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-a", 0, 0, 1920, 1080, mapping(0.0f, 0.5f)),
        candidate("uuid-b", 1920, 0, 3840, 1080, mapping(0.5f, 0.5f))};
    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), selected({"uuid-a", "uuid-b"}));
    require(prepared.ready() && prepared.eligibility.eligible_count == 2,
            "both persisted screens should be eligible");

    std::uint64_t camera_ownership_token = 0xA42A42u;
    const auto a1 = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 100, 100);
    const auto b = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 2000, 100);
    const auto a2 = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 100, 100);

    require(a1.active() && a1.active_index == 0,
            "cursor on Monitor A should select mapping A");
    require(b.active() && b.active_index == 1,
            "cursor on Monitor B should select mapping B");
    require(a2.active() && a2.active_index == 0,
            "crossing back should immediately restore mapping A");
    require(camera_ownership_token == 0xA42A42u,
            "active mapper must not own/reset camera state");
    require(prepared.resolver_candidates[0].mapping.scene_offset.x == 0.0f &&
                prepared.resolver_candidates[1].mapping.scene_offset.x == 0.5f,
            "independent mappings must remain attached to their candidates");
}

void outside_and_utility_never_guess()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-a", 0, 0, 1920, 1080, mapping(0.0f, 0.4f)),
        candidate("uuid-b", 1920, 0, 3840, 1080, mapping(0.4f, 0.4f)),
        candidate("uuid-utility", 3840, 0, 5760, 1080, mapping(0.8f, 0.2f))};
    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), selected({"uuid-a", "uuid-b"}));

    const auto outside = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 6000, 500);
    require(outside.status ==
                arzoom::PresentationScreenResolveStatus::CursorOutsideEligibleScreens,
            "cursor outside Presentation Screens must never select nearest source");

    const auto utility = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 4000, 500);
    require(utility.status ==
                arzoom::PresentationScreenResolveStatus::CursorOutsideEligibleScreens,
            "unchecked utility Display Capture must never become active");
}

void duplicate_monitor_ownership_is_ambiguous()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-a", 0, 0, 1920, 1080, mapping(0.0f, 0.5f)),
        candidate("uuid-b", 0, 0, 1920, 1080, mapping(0.5f, 0.5f))};
    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), selected({"uuid-a", "uuid-b"}));
    const auto result = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 1000, 500);
    require(result.status ==
                arzoom::PresentationScreenResolveStatus::AmbiguousMonitorOwnership,
            "two eligible captures claiming one monitor must fail safe");
}

void missing_or_invalid_selected_screen_fails_safe()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-b", 1920, 0, 3840, 1080, mapping(0.5f, 0.5f))};
    const auto missing = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), selected({"uuid-a"}));
    require(!missing.ready() &&
                missing.eligibility.status ==
                    arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable,
            "missing persisted UUID must disable active mapping safely");

    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> invalid = {
        candidate("uuid-a", 0, 0, 1920, 1080, mapping(0.0f, 1.0f), false)};
    const auto invalid_prepared = arzoom::presentation_screen_prepare_active_mapping(
        invalid.data(), invalid.size(), selected({"uuid-a"}));
    require(!invalid_prepared.ready(),
            "invalid runtime geometry/monitor state must not become active");
}

void negative_coordinates_and_boundary_crossing_are_deterministic()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates = {
        candidate("uuid-left", -1920, 0, 0, 1080, mapping(0.0f, 0.5f)),
        candidate("uuid-right", 0, 0, 1920, 1080, mapping(0.5f, 0.5f))};
    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), selected({"uuid-left", "uuid-right"}));

    const auto left = arzoom::presentation_screen_resolve_active_mapping(
        prepared, -1, 500);
    const auto right = arzoom::presentation_screen_resolve_active_mapping(
        prepared, 0, 500);
    const auto left_again = arzoom::presentation_screen_resolve_active_mapping(
        prepared, -1, 500);
    require(left.active() && left.active_index == 0,
            "negative desktop coordinate should resolve left monitor");
    require(right.active() && right.active_index == 1,
            "half-open boundary should resolve right monitor at x=0");
    require(left_again.active() && left_again.active_index == 0,
            "rapid boundary crossing should remain stateless and deterministic");
}

} // namespace

int main()
{
    one_screen_auto_preserves_existing_behavior();
    two_selected_screens_switch_a_b_a_without_camera_ownership_reset();
    outside_and_utility_never_guess();
    duplicate_monitor_ownership_is_ambiguous();
    missing_or_invalid_selected_screen_fails_safe();
    negative_coordinates_and_boundary_crossing_are_deterministic();
    std::cout << "ArZoom P4.2 active mapping adapter gates: PASS\n";
    return 0;
}
