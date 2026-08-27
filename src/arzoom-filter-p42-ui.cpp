#include "arzoom-filter-p42-active.cpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

/*
 * P4.2 Slice 8 — Beginner Presentation Screens UI + diagnostics
 * ==================================================================
 *
 * This final wrapper adds only a Properties-facing configuration layer around
 * the accepted Slice 5 runtime. Display Capture eligibility is still persisted
 * exclusively as source UUIDs. The row controls are explicit toggle buttons
 * rendered with checkbox glyphs, so no dynamic bool/name/index setting becomes
 * a second identity store.
 *
 * Runtime diagnostics observe the authoritative mapping after the inherited
 * active adapter tick. They never resolve Presentation Screens independently.
 * Properties refreshes are user-driven only (row/action/refresh button); cursor
 * movement never rebuilds Properties and therefore cannot reintroduce P5 GUI
 * flicker.
 */
namespace {

constexpr const char kPhase42UiTogglePrefix[] = "p42_ui_screen_toggle:";
constexpr const char kPhase42UiGroupName[] = "p42_presentation_screens_group";

struct Phase42UiRuntimeSnapshot {
    bool mapping_valid = false;
    bool refresh_pending = false;
    std::string active_source_uuid;
    std::string active_label;
    std::string active_monitor_label;
    std::string reason;
};

struct Phase42UiFilter {
    Phase42ActiveFilter *active = nullptr;
    std::mutex diagnostics_mutex;
    Phase42UiRuntimeSnapshot diagnostics{};
    std::string last_logged_state;
};

struct Phase42UiRow {
    std::string source_uuid;
    std::string display_label;
    std::string monitor_label;
    std::string reason;
    bool ready = false;
};

struct Phase42UiSetupSnapshot {
    bool relevant = false;
    std::string discovery_reason;
    arzoom::PresentationScreenSelectionSettings selection{};
    arzoom::PresentationScreenEligibilityResult eligibility{};
    std::vector<Phase42UiRow> rows;
    std::string implicit_auto_uuid;
};

enum class Phase42UiDiagnosticKind {
    Unavailable,
    Ready,
    Active,
    CursorOutside,
    NeedsSetup,
    AmbiguousMonitor,
    MissingHidden,
    MonitorUnresolved,
    TransformUnsupported,
    GeometryInvalid,
    AmbiguousIdentity,
    UnsupportedSchema,
    SelectionSaved,
};

Phase41Filter *phase42_ui_phase41(Phase42UiFilter *wrapper)
{
    return wrapper && wrapper->active
               ? phase42_active_phase41(wrapper->active)
               : nullptr;
}

ArZoomFilter *phase42_ui_phase1(Phase42UiFilter *wrapper)
{
    return phase1_from_phase41(phase42_ui_phase41(wrapper));
}

obs_source_t *phase42_ui_context(Phase42UiFilter *wrapper)
{
    ArZoomFilter *phase1 = phase42_ui_phase1(wrapper);
    return phase1 ? phase1->context : nullptr;
}

bool phase42_ui_same_monitor(const MonitorDescriptor &a,
                             const MonitorDescriptor &b)
{
    return a.left == b.left && a.top == b.top &&
           a.right == b.right && a.bottom == b.bottom;
}

bool phase42_ui_near(float a, float b)
{
    return std::fabs(a - b) <= 1.0e-6f;
}

bool phase42_ui_same_mapping(const arzoom::SceneAxisAlignedMapping &a,
                             const arzoom::SceneAxisAlignedMapping &b)
{
    return phase42_ui_near(a.source_visible_min.x, b.source_visible_min.x) &&
           phase42_ui_near(a.source_visible_min.y, b.source_visible_min.y) &&
           phase42_ui_near(a.source_visible_max.x, b.source_visible_max.x) &&
           phase42_ui_near(a.source_visible_max.y, b.source_visible_max.y) &&
           phase42_ui_near(a.scene_offset.x, b.scene_offset.x) &&
           phase42_ui_near(a.scene_offset.y, b.scene_offset.y) &&
           phase42_ui_near(a.scene_scale.x, b.scene_scale.x) &&
           phase42_ui_near(a.scene_scale.y, b.scene_scale.y);
}

void phase42_ui_capture_runtime(Phase42UiFilter *wrapper)
{
    if (!wrapper || !wrapper->active)
        return;

    Phase42UiRuntimeSnapshot next;
    Phase41Filter *phase41 = phase42_ui_phase41(wrapper);
    if (!phase41) {
        next.reason = "Presentation Screen mapping runtime is unavailable";
    } else if (!phase41->layout_mapping_valid) {
        next.reason = phase41->mapping_reason;
    } else {
        next.mapping_valid = true;
        next.active_monitor_label = phase41->physical_monitor.label;

        const SceneDisplayCandidateSnapshot *match = nullptr;
        std::size_t match_count = 0;
        for (const auto &candidate : wrapper->active->discovered_candidates) {
            if (!candidate.ready() ||
                !phase42_ui_same_monitor(candidate.physical_monitor,
                                         phase41->physical_monitor) ||
                !phase42_ui_same_mapping(candidate.mapping, phase41->mapping)) {
                continue;
            }
            match = &candidate;
            ++match_count;
        }
        if (match_count == 1 && match) {
            next.active_source_uuid = match->identity.source_uuid;
            next.active_label = match->identity.display_label;
        }
    }

    std::string state_key;
    if (next.mapping_valid) {
        state_key = "active:" + next.active_source_uuid + ":" +
                    next.active_monitor_label;
    } else {
        state_key = "reason:" + next.reason;
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(wrapper->diagnostics_mutex);
        changed = state_key != wrapper->last_logged_state;
        wrapper->diagnostics = next;
        if (changed)
            wrapper->last_logged_state = state_key;
    }

    if (!changed)
        return;

    if (next.mapping_valid) {
        const char *label = next.active_label.empty()
                                ? "Display Capture"
                                : next.active_label.c_str();
        blog(LOG_INFO,
             "[ArZoom] P4.2 active Presentation Screen: %s (%s)",
             label,
             next.active_monitor_label.empty()
                 ? "monitor mapped"
                 : next.active_monitor_label.c_str());
        return;
    }

    if (next.reason == "cursor outside Presentation Screens") {
        blog(LOG_INFO,
             "[ArZoom] P4.2 Presentation Screens: cursor is outside selected Presentation Screens");
    } else if (!next.reason.empty()) {
        blog(LOG_WARNING,
             "[ArZoom] P4.2 Presentation Screens: %s",
             next.reason.c_str());
    }
}

Phase42UiRuntimeSnapshot phase42_ui_runtime_snapshot(
    Phase42UiFilter *wrapper)
{
    Phase42UiRuntimeSnapshot snapshot;
    if (!wrapper)
        return snapshot;
    std::lock_guard<std::mutex> lock(wrapper->diagnostics_mutex);
    return wrapper->diagnostics;
}

void phase42_ui_mark_selection_saved(Phase42UiFilter *wrapper)
{
    if (!wrapper)
        return;
    std::lock_guard<std::mutex> lock(wrapper->diagnostics_mutex);
    wrapper->diagnostics = {};
    wrapper->diagnostics.refresh_pending = true;
    wrapper->last_logged_state.clear();
}

arzoom::PresentationScreenSelectionSettings phase42_ui_current_selection(
    Phase42UiFilter *wrapper)
{
    obs_source_t *context = phase42_ui_context(wrapper);
    if (!context)
        return {};

    obs_data_t *settings = obs_source_get_settings(context);
    if (!settings)
        return {};
    const auto selection = phase42_read_settings(settings);
    obs_data_release(settings);
    return selection;
}

bool phase42_ui_write_selection(
    Phase42UiFilter *wrapper,
    const arzoom::PresentationScreenSelectionSettings &selection)
{
    obs_source_t *context = phase42_ui_context(wrapper);
    if (!context)
        return false;

    obs_data_t *settings = obs_source_get_settings(context);
    if (!settings)
        return false;

    const auto wire = arzoom::presentation_screen_encode_selection(selection);
    obs_data_set_int(settings,
                     arzoom::kPresentationScreenSettingsSchemaKey,
                     arzoom::kPresentationScreenSettingsSchemaVersion);

    obs_data_array_t *array = obs_data_array_create();
    if (!array) {
        obs_data_release(settings);
        return false;
    }

    bool ok = true;
    for (const auto &uuid : wire.source_uuids) {
        obs_data_t *entry = obs_data_create();
        if (!entry) {
            ok = false;
            break;
        }
        obs_data_set_string(entry,
                            arzoom::kPresentationScreenSettingsUuidKey,
                            uuid.c_str());
        obs_data_array_push_back(array, entry);
        obs_data_release(entry);
    }

    if (ok) {
        obs_data_set_array(settings,
                           arzoom::kPresentationScreenSettingsArrayKey,
                           array);
        obs_source_update(context, settings);
        phase42_ui_mark_selection_saved(wrapper);
    }

    obs_data_array_release(array);
    obs_data_release(settings);
    return ok;
}

Phase42UiSetupSnapshot phase42_ui_collect_setup_snapshot(
    Phase42UiFilter *wrapper)
{
    Phase42UiSetupSnapshot snapshot;
    Phase41Filter *phase41 = phase42_ui_phase41(wrapper);
    ArZoomFilter *phase1 = phase1_from_phase41(phase41);
    obs_source_t *scene_source = nullptr;
    if (!phase41 || !is_managed_scene_camera(phase1, &scene_source))
        return snapshot;

    snapshot.relevant = true;
    snapshot.selection = phase42_ui_current_selection(wrapper);

    std::vector<SceneDisplayCandidateSnapshot> discovered;
    if (!discover_scene_display_candidates(
            scene_source, discovered, snapshot.discovery_reason)) {
        snapshot.eligibility = arzoom::presentation_screen_resolve_eligibility(
            nullptr, 0, snapshot.selection);
        return snapshot;
    }

    std::vector<arzoom::PresentationScreenActiveMappingCandidate> candidates;
    candidates.reserve(discovered.size());
    for (const auto &candidate : discovered)
        candidates.push_back(phase42_active_candidate(candidate));

    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), snapshot.selection);
    snapshot.eligibility = prepared.eligibility;

    snapshot.rows.reserve(discovered.size());
    for (const auto &candidate : discovered) {
        Phase42UiRow row;
        row.source_uuid = candidate.identity.source_uuid;
        row.display_label = candidate.identity.display_label;
        row.monitor_label = candidate.monitor_resolved
                                ? candidate.physical_monitor.label
                                : std::string{};
        row.reason = candidate.reason;
        row.ready = candidate.ready();
        snapshot.rows.push_back(std::move(row));
    }

    if (snapshot.eligibility.implicit_auto &&
        snapshot.eligibility.eligible.size() == discovered.size()) {
        for (std::size_t i = 0; i < discovered.size(); ++i) {
            if (snapshot.eligibility.eligible[i]) {
                snapshot.implicit_auto_uuid =
                    discovered[i].identity.source_uuid;
                break;
            }
        }
    }
    return snapshot;
}

Phase42UiDiagnosticKind phase42_ui_reason_kind(const std::string &reason)
{
    if (reason.find("monitor could not be resolved") != std::string::npos)
        return Phase42UiDiagnosticKind::MonitorUnresolved;
    if (reason.find("rotation/skew") != std::string::npos ||
        reason.find("bounds scaling") != std::string::npos ||
        reason.find("flipped/degenerate") != std::string::npos) {
        return Phase42UiDiagnosticKind::TransformUnsupported;
    }
    if (reason.find("invalid") != std::string::npos ||
        reason.find("crop") != std::string::npos ||
        reason.find("safe desktop-coordinate range") != std::string::npos ||
        reason.find("Display Capture size") != std::string::npos) {
        return Phase42UiDiagnosticKind::GeometryInvalid;
    }
    return Phase42UiDiagnosticKind::Unavailable;
}

Phase42UiDiagnosticKind phase42_ui_selected_failure_kind(
    const Phase42UiSetupSnapshot &snapshot)
{
    if (!snapshot.selection.persisted)
        return Phase42UiDiagnosticKind::Unavailable;

    for (const auto &uuid : snapshot.selection.selected_source_uuids) {
        const auto found = std::find_if(
            snapshot.rows.begin(), snapshot.rows.end(),
            [&](const Phase42UiRow &row) { return row.source_uuid == uuid; });
        if (found == snapshot.rows.end())
            return Phase42UiDiagnosticKind::MissingHidden;
        if (!found->ready)
            return phase42_ui_reason_kind(found->reason);
    }
    return Phase42UiDiagnosticKind::MissingHidden;
}

Phase42UiDiagnosticKind phase42_ui_setup_kind(
    const Phase42UiSetupSnapshot &snapshot)
{
    if (!snapshot.discovery_reason.empty()) {
        if (snapshot.selection.persisted)
            return Phase42UiDiagnosticKind::MissingHidden;
        return Phase42UiDiagnosticKind::Unavailable;
    }

    switch (snapshot.eligibility.status) {
    case arzoom::PresentationScreenEligibilityStatus::AutoSingle:
    case arzoom::PresentationScreenEligibilityStatus::ReadySelected:
        return Phase42UiDiagnosticKind::Ready;
    case arzoom::PresentationScreenEligibilityStatus::NeedsSetup:
        return Phase42UiDiagnosticKind::NeedsSetup;
    case arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable:
        return phase42_ui_selected_failure_kind(snapshot);
    case arzoom::PresentationScreenEligibilityStatus::AmbiguousIdentity:
        return Phase42UiDiagnosticKind::AmbiguousIdentity;
    case arzoom::PresentationScreenEligibilityStatus::UnsupportedSchema:
        return Phase42UiDiagnosticKind::UnsupportedSchema;
    case arzoom::PresentationScreenEligibilityStatus::Unavailable:
    default:
        for (const auto &row : snapshot.rows) {
            if (!row.ready)
                return phase42_ui_reason_kind(row.reason);
        }
        return Phase42UiDiagnosticKind::Unavailable;
    }
}

const char *phase42_ui_diagnostic_locale_key(Phase42UiDiagnosticKind kind)
{
    switch (kind) {
    case Phase42UiDiagnosticKind::Active:
        return "ArZoom.PresentationScreens.Status.Active";
    case Phase42UiDiagnosticKind::CursorOutside:
        return "ArZoom.PresentationScreens.Status.CursorOutside";
    case Phase42UiDiagnosticKind::NeedsSetup:
        return "ArZoom.PresentationScreens.Status.NeedsSetup";
    case Phase42UiDiagnosticKind::AmbiguousMonitor:
        return "ArZoom.PresentationScreens.Status.AmbiguousMonitor";
    case Phase42UiDiagnosticKind::MissingHidden:
        return "ArZoom.PresentationScreens.Status.MissingHidden";
    case Phase42UiDiagnosticKind::MonitorUnresolved:
        return "ArZoom.PresentationScreens.Status.MonitorUnresolved";
    case Phase42UiDiagnosticKind::TransformUnsupported:
        return "ArZoom.PresentationScreens.Status.TransformUnsupported";
    case Phase42UiDiagnosticKind::GeometryInvalid:
        return "ArZoom.PresentationScreens.Status.GeometryInvalid";
    case Phase42UiDiagnosticKind::AmbiguousIdentity:
        return "ArZoom.PresentationScreens.Status.AmbiguousIdentity";
    case Phase42UiDiagnosticKind::UnsupportedSchema:
        return "ArZoom.PresentationScreens.Status.UnsupportedSchema";
    case Phase42UiDiagnosticKind::SelectionSaved:
        return "ArZoom.PresentationScreens.Status.SelectionSaved";
    case Phase42UiDiagnosticKind::Ready:
        return "ArZoom.PresentationScreens.Status.Ready";
    case Phase42UiDiagnosticKind::Unavailable:
    default:
        return "ArZoom.PresentationScreens.Status.Unavailable";
    }
}

enum obs_text_info_type phase42_ui_info_type(Phase42UiDiagnosticKind kind)
{
    switch (kind) {
    case Phase42UiDiagnosticKind::Ready:
    case Phase42UiDiagnosticKind::Active:
    case Phase42UiDiagnosticKind::SelectionSaved:
        return OBS_TEXT_INFO_NORMAL;
    case Phase42UiDiagnosticKind::AmbiguousMonitor:
    case Phase42UiDiagnosticKind::TransformUnsupported:
    case Phase42UiDiagnosticKind::GeometryInvalid:
    case Phase42UiDiagnosticKind::AmbiguousIdentity:
    case Phase42UiDiagnosticKind::UnsupportedSchema:
        return OBS_TEXT_INFO_ERROR;
    case Phase42UiDiagnosticKind::Unavailable:
    case Phase42UiDiagnosticKind::CursorOutside:
    case Phase42UiDiagnosticKind::NeedsSetup:
    case Phase42UiDiagnosticKind::MissingHidden:
    case Phase42UiDiagnosticKind::MonitorUnresolved:
    default:
        return OBS_TEXT_INFO_WARNING;
    }
}

std::string phase42_ui_ready_text(std::size_t count)
{
    std::string text = obs_module_text("ArZoom.PresentationScreens.Status.Ready");
    text += " — ";
    text += std::to_string(count);
    text += " ";
    text += obs_module_text(
        count == 1
            ? "ArZoom.PresentationScreens.ScreenSingular"
            : "ArZoom.PresentationScreens.ScreenPlural");
    return text;
}

std::string phase42_ui_runtime_text(
    const Phase42UiRuntimeSnapshot &runtime)
{
    if (runtime.refresh_pending)
        return obs_module_text("ArZoom.PresentationScreens.Status.SelectionSaved");

    if (runtime.mapping_valid) {
        std::string text =
            obs_module_text("ArZoom.PresentationScreens.Status.Active");
        text += " — ";
        text += runtime.active_label.empty()
                    ? obs_module_text("ArZoom.PresentationScreens.Unnamed")
                    : runtime.active_label;
        if (!runtime.active_monitor_label.empty()) {
            text += " / ";
            text += runtime.active_monitor_label;
        }
        return text;
    }

    if (runtime.reason == "cursor outside Presentation Screens")
        return obs_module_text("ArZoom.PresentationScreens.Status.CursorOutside");
    if (runtime.reason ==
        "multiple eligible Presentation Screens claim the cursor monitor") {
        return obs_module_text("ArZoom.PresentationScreens.Status.AmbiguousMonitor");
    }
    if (!runtime.reason.empty()) {
        return obs_module_text(
            phase42_ui_diagnostic_locale_key(
                phase42_ui_reason_kind(runtime.reason)));
    }
    return obs_module_text("ArZoom.PresentationScreens.ActiveAutomatic");
}

Phase42UiDiagnosticKind phase42_ui_runtime_kind(
    const Phase42UiRuntimeSnapshot &runtime)
{
    if (runtime.refresh_pending)
        return Phase42UiDiagnosticKind::SelectionSaved;
    if (runtime.mapping_valid)
        return Phase42UiDiagnosticKind::Active;
    if (runtime.reason == "cursor outside Presentation Screens")
        return Phase42UiDiagnosticKind::CursorOutside;
    if (runtime.reason ==
        "multiple eligible Presentation Screens claim the cursor monitor") {
        return Phase42UiDiagnosticKind::AmbiguousMonitor;
    }
    if (!runtime.reason.empty())
        return phase42_ui_reason_kind(runtime.reason);
    return Phase42UiDiagnosticKind::Ready;
}

bool phase42_ui_row_checked(const Phase42UiSetupSnapshot &snapshot,
                            const Phase42UiRow &row)
{
    if (snapshot.selection.persisted) {
        return arzoom::presentation_screen_selection_contains(
            snapshot.selection, row.source_uuid);
    }
    return !snapshot.implicit_auto_uuid.empty() &&
           row.source_uuid == snapshot.implicit_auto_uuid;
}

std::string phase42_ui_row_text(const Phase42UiSetupSnapshot &snapshot,
                                const Phase42UiRow &row)
{
    std::string text = phase42_ui_row_checked(snapshot, row)
                           ? u8"\u2611 "
                           : u8"\u2610 ";
    text += row.display_label.empty()
                ? obs_module_text("ArZoom.PresentationScreens.Unnamed")
                : row.display_label;
    if (!row.monitor_label.empty()) {
        text += " (";
        text += row.monitor_label;
        text += ")";
    }
    if (!snapshot.selection.persisted &&
        row.source_uuid == snapshot.implicit_auto_uuid) {
        text += " — ";
        text += obs_module_text("ArZoom.PresentationScreens.Auto");
    }
    if (!row.ready) {
        text += " — ";
        text += obs_module_text(
            phase42_ui_diagnostic_locale_key(
                phase42_ui_reason_kind(row.reason)));
    }
    return text;
}

bool phase42_ui_screen_toggle_clicked(obs_properties_t *,
                                      obs_property_t *property,
                                      void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (!wrapper || !property)
        return false;

    const char *name = obs_property_name(property);
    if (!name)
        return false;
    const std::size_t prefix_length = std::strlen(kPhase42UiTogglePrefix);
    if (std::strncmp(name, kPhase42UiTogglePrefix, prefix_length) != 0)
        return false;
    const std::string uuid = name + prefix_length;
    if (uuid.empty())
        return false;

    const auto snapshot = phase42_ui_collect_setup_snapshot(wrapper);
    const auto found = std::find_if(
        snapshot.rows.begin(), snapshot.rows.end(),
        [&](const Phase42UiRow &row) { return row.source_uuid == uuid; });
    if (found == snapshot.rows.end())
        return false;

    const bool checked = phase42_ui_row_checked(snapshot, *found);
    const auto next = arzoom::presentation_screen_set_selected(
        snapshot.selection, uuid, !checked);
    return phase42_ui_write_selection(wrapper, next);
}

bool phase42_ui_select_all_clicked(obs_properties_t *, obs_property_t *,
                                   void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (!wrapper)
        return false;

    const auto snapshot = phase42_ui_collect_setup_snapshot(wrapper);
    std::vector<std::string> uuids;
    for (const auto &row : snapshot.rows) {
        if (row.ready && !row.source_uuid.empty())
            uuids.push_back(row.source_uuid);
    }
    if (uuids.empty())
        return false;

    return phase42_ui_write_selection(
        wrapper, arzoom::presentation_screen_replace_selection(uuids));
}

bool phase42_ui_clear_clicked(obs_properties_t *, obs_property_t *, void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (!wrapper)
        return false;
    return phase42_ui_write_selection(
        wrapper, arzoom::presentation_screen_replace_selection({}));
}

bool phase42_ui_refresh_status_clicked(obs_properties_t *, obs_property_t *,
                                       void *)
{
    /* Explicit user action only. Returning true rebuilds this Properties view
     * from the latest tick snapshot; cursor movement never calls this path. */
    return true;
}

obs_property_t *phase42_ui_add_status(obs_properties_t *props,
                                      const char *name,
                                      const std::string &text,
                                      Phase42UiDiagnosticKind kind)
{
    obs_property_t *status = obs_properties_add_text(
        props, name, text.c_str(), OBS_TEXT_INFO);
    obs_property_text_set_info_type(status, phase42_ui_info_type(kind));
    obs_property_text_set_info_word_wrap(status, true);
    return status;
}

obs_properties_t *phase42_ui_properties(void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    obs_properties_t *props = phase42_active_properties(
        wrapper ? wrapper->active : nullptr);
    if (!wrapper || !props)
        return props;

    const auto snapshot = phase42_ui_collect_setup_snapshot(wrapper);
    if (!snapshot.relevant)
        return props;

    obs_properties_t *screens = obs_properties_create();
    obs_property_t *info = obs_properties_add_text(
        screens, "p42_presentation_screens_info",
        obs_module_text("ArZoom.PresentationScreens.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    const Phase42UiDiagnosticKind setup_kind =
        phase42_ui_setup_kind(snapshot);
    std::string setup_text;
    if (setup_kind == Phase42UiDiagnosticKind::Ready) {
        setup_text = phase42_ui_ready_text(snapshot.eligibility.eligible_count);
        if (snapshot.eligibility.unavailable_selected_count > 0) {
            setup_text += " · ";
            setup_text += obs_module_text(
                "ArZoom.PresentationScreens.Status.SomeUnavailable");
        }
    } else {
        setup_text = obs_module_text(
            phase42_ui_diagnostic_locale_key(setup_kind));
    }
    phase42_ui_add_status(screens, "p42_presentation_screens_setup_status",
                          setup_text, setup_kind);

    if (snapshot.eligibility.ready()) {
        const auto runtime = phase42_ui_runtime_snapshot(wrapper);
        phase42_ui_add_status(
            screens, "p42_presentation_screens_runtime_status",
            phase42_ui_runtime_text(runtime),
            phase42_ui_runtime_kind(runtime));
    }

    for (const auto &row : snapshot.rows) {
        if (row.source_uuid.empty())
            continue;
        const std::string property_name =
            std::string(kPhase42UiTogglePrefix) + row.source_uuid;
        const bool checked = phase42_ui_row_checked(snapshot, row);
        obs_property_t *toggle = obs_properties_add_button2(
            screens, property_name.c_str(),
            phase42_ui_row_text(snapshot, row).c_str(),
            phase42_ui_screen_toggle_clicked, wrapper);
        if (!row.ready && !checked)
            obs_property_set_enabled(toggle, false);
    }

    obs_property_t *select_all = obs_properties_add_button2(
        screens, "p42_presentation_screens_select_all",
        obs_module_text("ArZoom.PresentationScreens.SelectAll"),
        phase42_ui_select_all_clicked, wrapper);
    const bool has_ready_row = std::any_of(
        snapshot.rows.begin(), snapshot.rows.end(),
        [](const Phase42UiRow &row) { return row.ready; });
    obs_property_set_enabled(select_all, has_ready_row);

    obs_properties_add_button2(
        screens, "p42_presentation_screens_clear",
        obs_module_text("ArZoom.PresentationScreens.Clear"),
        phase42_ui_clear_clicked, wrapper);
    obs_properties_add_button2(
        screens, "p42_presentation_screens_refresh",
        obs_module_text("ArZoom.PresentationScreens.RefreshStatus"),
        phase42_ui_refresh_status_clicked, wrapper);

    obs_properties_add_group(
        props, kPhase42UiGroupName,
        obs_module_text("ArZoom.PresentationScreens.Group"),
        OBS_GROUP_NORMAL, screens);
    return props;
}

void phase42_ui_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (!wrapper || !wrapper->active)
        return;
    phase42_active_tick(wrapper->active, seconds);
    phase42_ui_capture_runtime(wrapper);
}

void phase42_ui_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (wrapper && wrapper->active)
        phase42_active_update(wrapper->active, settings);
}

void phase42_ui_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (wrapper && wrapper->active)
        phase42_active_render(wrapper->active, effect);
}

void phase42_ui_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (wrapper && wrapper->active)
        phase42_active_deactivate(wrapper->active);
}

void phase42_ui_destroy(void *data)
{
    auto *wrapper = static_cast<Phase42UiFilter *>(data);
    if (!wrapper)
        return;
    phase42_active_destroy(wrapper->active);
    delete wrapper;
}

void *phase42_ui_create(obs_data_t *settings, obs_source_t *context)
{
    auto *active = static_cast<Phase42ActiveFilter *>(
        phase42_active_create(settings, context));
    if (!active)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase42UiFilter();
    if (!wrapper) {
        phase42_active_destroy(active);
        return nullptr;
    }
    wrapper->active = active;
    phase42_ui_capture_runtime(wrapper);

    blog(LOG_INFO,
         "[ArZoom] P4.2 Presentation Screens beginner UI ready");
    return wrapper;
}

struct Phase42UiSourceInfoOverride {
    Phase42UiSourceInfoOverride()
    {
        arzoom_filter_info.create = phase42_ui_create;
        arzoom_filter_info.destroy = phase42_ui_destroy;
        arzoom_filter_info.video_tick = phase42_ui_tick;
        arzoom_filter_info.video_render = phase42_ui_render;
        arzoom_filter_info.update = phase42_ui_update;
        arzoom_filter_info.get_properties = phase42_ui_properties;
        arzoom_filter_info.get_defaults = phase42_settings_defaults;
        arzoom_filter_info.deactivate = phase42_ui_deactivate;
    }
};

Phase42UiSourceInfoOverride phase42_ui_source_info_override;

} // namespace
