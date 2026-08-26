#include "arzoom-presentation-screen-settings.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

arzoom::PresentationScreenEligibilityCandidate candidate(
    const char *uuid, const char *label, bool ready = true)
{
    arzoom::PresentationScreenEligibilityCandidate result;
    result.identity.source_uuid = uuid ? uuid : "";
    result.identity.display_label = label ? label : "";
    result.identity.source_resolved = uuid && *uuid;
    result.ready = ready;
    return result;
}

arzoom::PresentationScreenSelectionSettings persisted(
    std::initializer_list<const char *> uuids)
{
    arzoom::PresentationScreenSelectionSettings settings;
    settings.persisted = true;
    for (const char *uuid : uuids)
        settings.selected_source_uuids.emplace_back(uuid ? uuid : "");
    return settings;
}

void implicit_auto_requires_exactly_one_ready_screen()
{
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> one = {
        candidate("uuid-a", "Coding")};
    const auto one_result = arzoom::presentation_screen_resolve_eligibility(
        one.data(), one.size(), {});
    require(one_result.status == arzoom::PresentationScreenEligibilityStatus::AutoSingle,
            "one ready capture without persisted selection should use implicit Auto");
    require(one_result.eligible_count == 1 && one_result.eligible[0] &&
                one_result.implicit_auto,
            "implicit Auto should mark exactly one eligible capture");

    const std::vector<arzoom::PresentationScreenEligibilityCandidate> two = {
        candidate("uuid-a", "Coding"), candidate("uuid-b", "App")};
    const auto two_result = arzoom::presentation_screen_resolve_eligibility(
        two.data(), two.size(), {});
    require(two_result.status == arzoom::PresentationScreenEligibilityStatus::NeedsSetup,
            "multiple ready captures without persisted selection must need setup");
    require(two_result.eligible_count == 0,
            "Needs setup must never guess all visible captures");
}

void explicit_uuid_selection_controls_eligibility()
{
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> candidates = {
        candidate("uuid-a", "Coding"), candidate("uuid-b", "Application"),
        candidate("uuid-c", "OBS Utility")};
    const auto settings = persisted({"uuid-a", "uuid-b"});
    const auto result = arzoom::presentation_screen_resolve_eligibility(
        candidates.data(), candidates.size(), settings);
    require(result.status == arzoom::PresentationScreenEligibilityStatus::ReadySelected,
            "persisted valid UUIDs should produce ready selected screens");
    require(result.eligible_count == 2 && result.eligible[0] && result.eligible[1] &&
                !result.eligible[2],
            "utility capture not present in persisted UUID list must remain excluded");
    require(!result.implicit_auto,
            "persisted selection must never be reported as implicit Auto");
}

void rename_and_reorder_preserve_uuid_selection()
{
    const auto settings = persisted({"uuid-b"});
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> before = {
        candidate("uuid-a", "Coding"), candidate("uuid-b", "Application")};
    const auto before_result = arzoom::presentation_screen_resolve_eligibility(
        before.data(), before.size(), settings);
    require(before_result.eligible[1], "UUID B should be selected before rename");

    const std::vector<arzoom::PresentationScreenEligibilityCandidate> after = {
        candidate("uuid-b", "Renamed App"), candidate("uuid-a", "Coding")};
    const auto after_result = arzoom::presentation_screen_resolve_eligibility(
        after.data(), after.size(), settings);
    require(after_result.status == arzoom::PresentationScreenEligibilityStatus::ReadySelected &&
                after_result.eligible[0] && !after_result.eligible[1],
            "rename and scene-item reorder must not change UUID-based selection");
}

void delete_recreate_does_not_inherit_explicit_selection()
{
    const auto settings = persisted({"uuid-old"});
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> recreated = {
        candidate("uuid-new", "Recreated Coding")};
    const auto result = arzoom::presentation_screen_resolve_eligibility(
        recreated.data(), recreated.size(), settings);
    require(result.status ==
                arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable,
            "new UUID must not inherit deleted source selection");
    require(result.eligible_count == 0 && !result.implicit_auto,
            "explicit stale selection must suppress implicit Auto inheritance");
}

void hidden_or_invalid_selected_screen_recovers_by_same_uuid()
{
    const auto settings = persisted({"uuid-a"});
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> hidden = {
        candidate("uuid-b", "Utility")};
    const auto hidden_result = arzoom::presentation_screen_resolve_eligibility(
        hidden.data(), hidden.size(), settings);
    require(hidden_result.status ==
                arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable,
            "missing selected UUID should fail safe");

    const std::vector<arzoom::PresentationScreenEligibilityCandidate> restored = {
        candidate("uuid-a", "Coding restored"), candidate("uuid-b", "Utility")};
    const auto restored_result = arzoom::presentation_screen_resolve_eligibility(
        restored.data(), restored.size(), settings);
    require(restored_result.status ==
                arzoom::PresentationScreenEligibilityStatus::ReadySelected &&
                restored_result.eligible[0],
            "restoring the same UUID should deterministically recover eligibility");

    const std::vector<arzoom::PresentationScreenEligibilityCandidate> invalid = {
        candidate("uuid-a", "Coding", false)};
    const auto invalid_result = arzoom::presentation_screen_resolve_eligibility(
        invalid.data(), invalid.size(), settings);
    require(invalid_result.status ==
                arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable,
            "selected capture with invalid geometry/monitor state must remain unavailable");
}

void duplicate_settings_are_normalized_and_duplicate_identity_is_ambiguous()
{
    const auto normalized = arzoom::presentation_screen_normalize_selected_uuids(
        {"uuid-a", "", "uuid-a", "uuid-b", "uuid-b"});
    require(normalized.size() == 2 && normalized[0] == "uuid-a" &&
                normalized[1] == "uuid-b",
            "persisted UUID list should remove empty and duplicate entries deterministically");

    const auto settings = persisted({"uuid-a"});
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> duplicate = {
        candidate("uuid-a", "A1"), candidate("uuid-a", "A2")};
    const auto result = arzoom::presentation_screen_resolve_eligibility(
        duplicate.data(), duplicate.size(), settings);
    require(result.status == arzoom::PresentationScreenEligibilityStatus::AmbiguousIdentity &&
                result.eligible_count == 0,
            "duplicate discovered UUID ownership must fail safe");
}

void settings_wire_round_trip_preserves_explicit_uuid_selection()
{
    auto original = persisted({"uuid-b", "uuid-a", "uuid-b", ""});
    const auto wire = arzoom::presentation_screen_encode_selection(original);
    require(wire.persisted &&
                wire.schema_version == arzoom::kPresentationScreenSettingsSchemaVersion,
            "wire encoding should retain persisted/schema state");
    require(wire.source_uuids.size() == 2 && wire.source_uuids[0] == "uuid-b" &&
                wire.source_uuids[1] == "uuid-a",
            "wire encoding should preserve ordered UUID identity while normalizing duplicates");

    const auto restored = arzoom::presentation_screen_decode_selection(wire);
    require(restored.persisted &&
                restored.selected_source_uuids == wire.source_uuids,
            "settings decode should restore the same persisted UUID selection");

    auto explicit_empty = persisted({});
    const auto empty_round_trip = arzoom::presentation_screen_decode_selection(
        arzoom::presentation_screen_encode_selection(explicit_empty));
    require(empty_round_trip.persisted &&
                empty_round_trip.selected_source_uuids.empty(),
            "explicit empty selection must remain distinguishable from no persisted selection");
}

void unsupported_schema_fails_safe()
{
    auto settings = persisted({"uuid-a"});
    settings.schema_version = arzoom::kPresentationScreenSettingsSchemaVersion + 1;
    const std::vector<arzoom::PresentationScreenEligibilityCandidate> candidates = {
        candidate("uuid-a", "Coding")};
    const auto result = arzoom::presentation_screen_resolve_eligibility(
        candidates.data(), candidates.size(), settings);
    require(result.status == arzoom::PresentationScreenEligibilityStatus::UnsupportedSchema &&
                result.eligible_count == 0,
            "future/unsupported settings schema must fail safe");
}

} // namespace

int main()
{
    implicit_auto_requires_exactly_one_ready_screen();
    explicit_uuid_selection_controls_eligibility();
    rename_and_reorder_preserve_uuid_selection();
    delete_recreate_does_not_inherit_explicit_selection();
    hidden_or_invalid_selected_screen_recovers_by_same_uuid();
    duplicate_settings_are_normalized_and_duplicate_identity_is_ambiguous();
    settings_wire_round_trip_preserves_explicit_uuid_selection();
    unsupported_schema_fails_safe();
    std::cout << "ArZoom Phase 4.2 Presentation Screens settings model: PASS\n";
    return 0;
}
