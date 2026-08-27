#include "arzoom-presentation-screen-settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef ARZOOM_SOURCE_ROOT
#error ARZOOM_SOURCE_ROOT must point at the repository root
#endif

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::string read_source(const char *relative_path)
{
    const std::string path =
        std::string(ARZOOM_SOURCE_ROOT) + "/" + relative_path;
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "could not read source contract file: " + path);

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string compact_source(std::string text)
{
    text.erase(
        std::remove_if(
            text.begin(), text.end(),
            [](unsigned char c) { return std::isspace(c) != 0; }),
        text.end());
    return text;
}

void require_contains(const std::string &text, const std::string &needle,
                      const char *message)
{
    require(text.find(needle) != std::string::npos, message);
}

void require_not_contains(const std::string &text, const std::string &needle,
                          const char *message)
{
    require(text.find(needle) == std::string::npos, message);
}

void uuid_selection_helpers_are_explicit_and_normalized()
{
    arzoom::PresentationScreenSelectionSettings implicit;
    require(!implicit.persisted,
            "fresh selection must retain implicit Auto semantics");

    const auto explicit_empty = arzoom::presentation_screen_set_selected(
        implicit, "uuid-a", false);
    require(explicit_empty.persisted,
            "unchecking implicit Auto must create an explicit selection model");
    require(explicit_empty.selected_source_uuids.empty(),
            "unchecking implicit Auto must persist an explicit empty list");

    const auto selected_a = arzoom::presentation_screen_set_selected(
        implicit, "uuid-a", true);
    require(selected_a.persisted &&
                selected_a.selected_source_uuids.size() == 1 &&
                selected_a.selected_source_uuids.front() == "uuid-a",
            "checking a row must persist its UUID only");
    require(arzoom::presentation_screen_selection_contains(
                selected_a, "uuid-a"),
            "selection lookup must use UUID identity");

    const auto selected_b = arzoom::presentation_screen_set_selected(
        selected_a, "uuid-b", true);
    const auto duplicate_b = arzoom::presentation_screen_set_selected(
        selected_b, "uuid-b", true);
    require(duplicate_b.selected_source_uuids.size() == 2,
            "rechecking a selected UUID must not create duplicates");

    const auto removed_a = arzoom::presentation_screen_set_selected(
        duplicate_b, "uuid-a", false);
    require(removed_a.selected_source_uuids.size() == 1 &&
                removed_a.selected_source_uuids.front() == "uuid-b",
            "unchecking a row must remove only that UUID");

    const auto replaced = arzoom::presentation_screen_replace_selection(
        {"uuid-b", "", "uuid-b", "uuid-c"});
    require(replaced.persisted,
            "Select all / Clear actions must create explicit settings");
    require(replaced.schema_version ==
                arzoom::kPresentationScreenSettingsSchemaVersion,
            "UI writes must normalize to the current schema");
    require(replaced.selected_source_uuids.size() == 2 &&
                replaced.selected_source_uuids[0] == "uuid-b" &&
                replaced.selected_source_uuids[1] == "uuid-c",
            "replacement selection must remove empty/duplicate UUIDs");

    const auto wire = arzoom::presentation_screen_encode_selection(replaced);
    const auto round_trip = arzoom::presentation_screen_decode_selection(wire);
    require(round_trip.persisted &&
                round_trip.selected_source_uuids ==
                    replaced.selected_source_uuids,
            "UI selection must retain the existing UUID settings round trip");
}

void properties_ui_source_contract_is_safe()
{
    const std::string ui = compact_source(
        read_source("src/arzoom-filter-p42-ui.cpp"));
    const std::string cmake = compact_source(
        read_source("CMakeLists.txt"));
    const std::string en = read_source("data/locale/en-US.ini");
    const std::string id = read_source("data/locale/id-ID.ini");

    require_contains(
        ui, "#include\"arzoom-filter-p42-active.cpp\"",
        "Slice 8 must wrap the accepted active adapter rather than replace it");
    require_contains(
        ui,
        "phase42_active_tick(wrapper->active,seconds);phase42_ui_capture_runtime(wrapper);",
        "diagnostics must observe the authoritative mapping after the active tick");
    require_not_contains(
        ui, "presentation_screen_resolve_active_mapping(",
        "Properties diagnostics must not create a second active-screen resolver");

    require_contains(
        ui,
        "std::string(kPhase42UiTogglePrefix)+row.source_uuid",
        "screen row control identity must be encoded from source UUID");
    require_contains(
        ui, "obs_property_name(property)",
        "row callback must recover the durable UUID from its button identity");
    require_contains(
        ui,
        "obs_data_set_string(entry,arzoom::kPresentationScreenSettingsUuidKey,uuid.c_str());",
        "persisted row identity must be written only through source_uuid");
    require_contains(
        ui,
        "obs_data_set_array(settings,arzoom::kPresentationScreenSettingsArrayKey,array);",
        "Presentation Screens must remain one persisted UUID array");
    require_contains(
        ui, "obs_source_update(context,settings);",
        "explicit user selection must flow through the normal OBS update path");
    require_not_contains(
        ui, "obs_data_set_bool(",
        "checkbox-like rows must not create a parallel dynamic bool settings store");
    require_not_contains(
        ui, "obs_source_update_properties(",
        "cursor/runtime changes must never force Properties refreshes");

    require_contains(
        ui, "u8\"\\u2611\"", 
        "selected rows must render a checkbox-style selected glyph");
    require_contains(
        ui, "u8\"\\u2610\"", 
        "unselected rows must render a checkbox-style empty glyph");
    require_contains(
        ui, "phase42_ui_select_all_clicked",
        "beginner UI must offer explicit select-all setup");
    require_contains(
        ui, "phase42_ui_clear_clicked",
        "beginner UI must offer a clear-selection recovery action");
    require_contains(
        ui, "phase42_ui_refresh_status_clicked",
        "runtime status refresh must remain explicit and user-driven");

    require_contains(
        cmake, "src/arzoom-filter-p42-ui.cpp",
        "plugin translation unit must advance to the Slice 8 wrapper");
    require_not_contains(
        cmake, "src/arzoom-filter-p42-active.cpp",
        "active adapter must not compile twice after the Slice 8 wrapper includes it");

    for (const std::string key : {
             "ArZoom.PresentationScreens.Group=",
             "ArZoom.PresentationScreens.Info=",
             "ArZoom.PresentationScreens.Status.NeedsSetup=",
             "ArZoom.PresentationScreens.Status.CursorOutside=",
             "ArZoom.PresentationScreens.Status.AmbiguousMonitor=",
             "ArZoom.PresentationScreens.Status.MissingHidden=",
             "ArZoom.PresentationScreens.Status.MonitorUnresolved=",
             "ArZoom.PresentationScreens.Status.TransformUnsupported=",
             "ArZoom.PresentationScreens.Status.GeometryInvalid=",
         }) {
        require_contains(en, key, "English Presentation Screens locale is incomplete");
        require_contains(id, key, "Indonesian Presentation Screens locale is incomplete");
    }
}

} // namespace

int main()
{
    uuid_selection_helpers_are_explicit_and_normalized();
    properties_ui_source_contract_is_safe();
    std::cout << "ArZoom Phase 4.2 beginner Presentation Screens UI: PASS\n";
    return 0;
}
