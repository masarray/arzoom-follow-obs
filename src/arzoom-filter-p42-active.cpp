#include "arzoom-filter-p42-settings.cpp"
#include "arzoom-presentation-screen-active-mapping.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/*
 * P4.2 Slice 5 — Active mapping adapter
 * ======================================
 * Candidate discovery + UUID eligibility + the pure resolver select exactly
 * one mapping. Display Captures remain coordinate references only; the accepted
 * scene-level camera/planner/kinematics remain the sole motion owners.
 *
 * Structural discovery stays at 0.25 s. Cursor ownership resolves every video
 * tick from the prepared set. The selected candidate is installed into the
 * existing Phase41Filter seam before the inherited tick, so Smart Follow,
 * click, Presentation Cursor and Spotlight all consume the same phase1 monitor.
 */
namespace {

constexpr float kPhase42ActiveRefreshSeconds = kPhase41MappingRefreshSeconds;

struct Phase42ActiveFilter {
    Phase42SettingsFilter *settings = nullptr;
    float discovery_refresh_elapsed = 1.0f;
    std::vector<SceneDisplayCandidateSnapshot> discovered_candidates;
    arzoom::PresentationScreenActiveMappingSet prepared{};
    std::string discovery_reason;
    std::string last_discovery_signature;
};

Phase41Filter *phase42_active_phase41(Phase42ActiveFilter *wrapper)
{
    if (!wrapper || !wrapper->settings || !wrapper->settings->phase24 ||
        !wrapper->settings->phase24->phase23 ||
        !wrapper->settings->phase24->phase23->phase51 ||
        !wrapper->settings->phase24->phase23->phase51->phase5) {
        return nullptr;
    }
    return wrapper->settings->phase24->phase23->phase51->phase5->phase41;
}

const char *phase42_eligibility_reason(
    arzoom::PresentationScreenEligibilityStatus status)
{
    switch (status) {
    case arzoom::PresentationScreenEligibilityStatus::Unavailable:
        return "no ready Presentation Screens";
    case arzoom::PresentationScreenEligibilityStatus::AutoSingle:
    case arzoom::PresentationScreenEligibilityStatus::ReadySelected:
        return "ready";
    case arzoom::PresentationScreenEligibilityStatus::NeedsSetup:
        return "Presentation Screens need setup; multiple captures are available but no persisted selection exists";
    case arzoom::PresentationScreenEligibilityStatus::SelectedScreensUnavailable:
        return "selected Presentation Screen is missing, hidden, or invalid";
    case arzoom::PresentationScreenEligibilityStatus::AmbiguousIdentity:
        return "selected Presentation Screen UUID is ambiguous";
    case arzoom::PresentationScreenEligibilityStatus::UnsupportedSchema:
        return "Presentation Screens settings schema is unsupported";
    }
    return "Presentation Screens unavailable";
}

const char *phase42_resolve_reason(arzoom::PresentationScreenResolveStatus status)
{
    switch (status) {
    case arzoom::PresentationScreenResolveStatus::Active:
        return "active";
    case arzoom::PresentationScreenResolveStatus::NoEligibleScreens:
        return "no eligible Presentation Screens";
    case arzoom::PresentationScreenResolveStatus::CursorOutsideEligibleScreens:
        return "cursor outside Presentation Screens";
    case arzoom::PresentationScreenResolveStatus::AmbiguousMonitorOwnership:
        return "multiple eligible Presentation Screens claim the cursor monitor";
    case arzoom::PresentationScreenResolveStatus::ActiveScreenInvalid:
        return "active Presentation Screen mapping is invalid";
    }
    return "Presentation Screen mapping unavailable";
}

#ifdef _WIN32
struct Phase42ObsMonitorResolveContext {
    const char *monitor_id = nullptr;
    MonitorDescriptor resolved{};
    std::size_t matches = 0;
};

bool phase42_monitor_identity_equal(const char *left, const char *right)
{
    return left && right && *left && *right && _stricmp(left, right) == 0;
}

BOOL CALLBACK phase42_resolve_obs_monitor_cb(HMONITOR handle, HDC,
                                              LPRECT rect, LPARAM param)
{
    auto *context = reinterpret_cast<Phase42ObsMonitorResolveContext *>(param);
    if (!context || !context->monitor_id || !rect)
        return TRUE;

    MONITORINFOEXA info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(handle, reinterpret_cast<LPMONITORINFO>(&info)))
        return TRUE;

    DISPLAY_DEVICEA device = {};
    device.cb = sizeof(device);
    const bool have_interface_id =
        EnumDisplayDevicesA(info.szDevice, 0, &device,
                            EDD_GET_DEVICE_INTERFACE_NAME) != FALSE;

    const bool matches_interface =
        have_interface_id &&
        phase42_monitor_identity_equal(context->monitor_id, device.DeviceID);
    const bool matches_gdi_name =
        phase42_monitor_identity_equal(context->monitor_id, info.szDevice);
    if (!matches_interface && !matches_gdi_name)
        return TRUE;

    MonitorDescriptor monitor;
    monitor.left = rect->left;
    monitor.top = rect->top;
    monitor.right = rect->right;
    monitor.bottom = rect->bottom;
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitor.device_name = info.szDevice;
    if (have_interface_id)
        monitor.device_id = device.DeviceID;
    monitor.label = monitor.device_name + "  ·  " +
                    std::to_string(monitor.width()) + "x" +
                    std::to_string(monitor.height()) + "  @ " +
                    std::to_string(monitor.left) + "," +
                    std::to_string(monitor.top);
    if (monitor.primary)
        monitor.label += "  ·  Primary";

    ++context->matches;
    if (context->matches == 1)
        context->resolved = std::move(monitor);
    return TRUE;
}

bool phase42_resolve_obs_monitor_id(const char *monitor_id,
                                    MonitorDescriptor &resolved)
{
    if (!monitor_id || !*monitor_id ||
        phase42_monitor_identity_equal(monitor_id, "DUMMY")) {
        return false;
    }

    Phase42ObsMonitorResolveContext context;
    context.monitor_id = monitor_id;
    EnumDisplayMonitors(nullptr, nullptr, phase42_resolve_obs_monitor_cb,
                        reinterpret_cast<LPARAM>(&context));
    if (context.matches != 1 || !context.resolved.valid())
        return false;

    resolved = context.resolved;
    return true;
}
#else
bool phase42_resolve_obs_monitor_id(const char *, MonitorDescriptor &)
{
    return false;
}
#endif

bool phase42_active_reconcile_candidate_monitor(
    SceneDisplayCandidateSnapshot &candidate)
{
    if (!candidate.identity.valid())
        return false;

    obs_source_t *source = obs_get_source_by_uuid(
        candidate.identity.source_uuid.c_str());
    if (!source)
        return false;

    const bool display_capture = is_display_capture(source);
    obs_data_t *settings = display_capture ? obs_source_get_settings(source)
                                           : nullptr;
    const char *monitor_id = settings
                                 ? obs_data_get_string(settings, "monitor_id")
                                 : nullptr;
    const bool authoritative_monitor_id =
        monitor_id && *monitor_id && std::strcmp(monitor_id, "DUMMY") != 0;

    MonitorDescriptor resolved;
    const bool direct_resolved =
        authoritative_monitor_id &&
        phase42_resolve_obs_monitor_id(monitor_id, resolved);

    if (settings)
        obs_data_release(settings);
    obs_source_release(source);

    /* Legacy Display Capture paths without monitor_id keep the already-proven
     * P4.1 resolver. Modern OBS monitor_capture uses monitor_id, so mirror the
     * same Win32 identity path OBS 32.x uses instead of silently falling back
     * to another same-sized display. */
    if (!authoritative_monitor_id)
        return candidate.monitor_resolved;

    if (!direct_resolved) {
        candidate.monitor_resolved = false;
        candidate.physical_monitor = {};
        candidate.mapped_monitor = {};
        candidate.reason =
            "Display Capture monitor_id could not be resolved deterministically";
        return false;
    }

    candidate.physical_monitor = resolved;
    candidate.monitor_resolved = true;

    if (candidate.geometry_valid && candidate.mapping.valid()) {
        if (!build_mapped_monitor(candidate.physical_monitor,
                                  candidate.mapping,
                                  candidate.mapped_monitor)) {
            candidate.reason =
                "scene mapping exceeded safe desktop-coordinate range";
            return false;
        }
        candidate.reason.clear();
    }
    return candidate.monitor_resolved;
}

arzoom::PresentationScreenActiveMappingCandidate phase42_active_candidate(
    const SceneDisplayCandidateSnapshot &candidate)
{
    arzoom::PresentationScreenActiveMappingCandidate result;
    result.identity = candidate.identity;
    result.physical_monitor = {
        static_cast<std::int64_t>(candidate.physical_monitor.left),
        static_cast<std::int64_t>(candidate.physical_monitor.top),
        static_cast<std::int64_t>(candidate.physical_monitor.right),
        static_cast<std::int64_t>(candidate.physical_monitor.bottom),
    };
    result.mapping = candidate.mapping;
    result.visible = candidate.visible;
    result.monitor_resolved = candidate.monitor_resolved;
    result.geometry_valid = candidate.geometry_valid;
    result.runtime_ready = candidate.ready();
    return result;
}

std::string phase42_active_discovery_signature(
    const std::vector<SceneDisplayCandidateSnapshot> &candidates,
    const std::string &reason)
{
    std::string signature = reason;
    for (const auto &candidate : candidates) {
        signature += "|" + candidate.identity.source_uuid + ":";
        signature += std::to_string(candidate.physical_monitor.left) + ",";
        signature += std::to_string(candidate.physical_monitor.top) + ",";
        signature += std::to_string(candidate.physical_monitor.right) + ",";
        signature += std::to_string(candidate.physical_monitor.bottom) + ":";
        signature += candidate.ready() ? "ready" : candidate.reason;
    }
    return signature;
}

void phase42_active_log_discovery_if_changed(Phase42ActiveFilter *wrapper)
{
    if (!wrapper)
        return;

    const std::string signature = phase42_active_discovery_signature(
        wrapper->discovered_candidates, wrapper->discovery_reason);
    if (signature == wrapper->last_discovery_signature)
        return;
    wrapper->last_discovery_signature = signature;

    if (!wrapper->discovery_reason.empty()) {
        blog(LOG_WARNING, "[ArZoom] P4.2 candidate discovery: %s",
             wrapper->discovery_reason.c_str());
        return;
    }

    for (const auto &candidate : wrapper->discovered_candidates) {
        blog(candidate.ready() ? LOG_INFO : LOG_WARNING,
             "[ArZoom] P4.2 candidate '%s': monitor=%s rect=%ld,%ld..%ld,%ld ready=%s%s%s",
             candidate.identity.display_label.empty()
                 ? "Display Capture"
                 : candidate.identity.display_label.c_str(),
             candidate.physical_monitor.device_name.empty()
                 ? "unresolved"
                 : candidate.physical_monitor.device_name.c_str(),
             candidate.physical_monitor.left,
             candidate.physical_monitor.top,
             candidate.physical_monitor.right,
             candidate.physical_monitor.bottom,
             candidate.ready() ? "yes" : "no",
             candidate.reason.empty() ? "" : " reason=",
             candidate.reason.empty() ? "" : candidate.reason.c_str());
    }
}

void phase42_active_refresh_candidates(Phase42ActiveFilter *wrapper,
                                       obs_source_t *scene_source)
{
    if (!wrapper || !wrapper->settings)
        return;

    wrapper->discovered_candidates.clear();
    wrapper->prepared = {};
    wrapper->discovery_reason.clear();

    if (!discover_scene_display_candidates(
            scene_source, wrapper->discovered_candidates,
            wrapper->discovery_reason)) {
        phase42_active_log_discovery_if_changed(wrapper);
        return;
    }

    for (auto &candidate : wrapper->discovered_candidates)
        phase42_active_reconcile_candidate_monitor(candidate);

    phase42_active_log_discovery_if_changed(wrapper);

    std::vector<arzoom::PresentationScreenActiveMappingCandidate> active_candidates;
    active_candidates.reserve(wrapper->discovered_candidates.size());
    for (const auto &candidate : wrapper->discovered_candidates)
        active_candidates.push_back(phase42_active_candidate(candidate));

    wrapper->prepared = arzoom::presentation_screen_prepare_active_mapping(
        active_candidates.data(), active_candidates.size(),
        wrapper->settings->presentation_screens);
}

void phase42_active_apply_native_cursor_warning(
    Phase41Filter *phase41,
    const SceneDisplayCandidateSnapshot &candidate)
{
    if (!phase41 || !phase41->phase4)
        return;

    if (candidate.native_cursor_enabled &&
        !phase41->phase4->nested_cursor_warning_logged) {
        blog(LOG_WARNING,
             "[ArZoom] Scene Camera: Display Capture native cursor is enabled. "
             "Turn it off when using an ArZoom Presentation Cursor to avoid double cursor.");
        phase41->phase4->nested_cursor_warning_logged = true;
    } else if (!candidate.native_cursor_enabled) {
        phase41->phase4->nested_cursor_warning_logged = false;
    }
}

bool phase42_active_select_mapping(Phase42ActiveFilter *wrapper,
                                   Phase41Filter *phase41)
{
    if (!wrapper || !phase41)
        return false;

    if (!wrapper->discovery_reason.empty()) {
        phase41->layout_mapping_valid = false;
        phase41->mapping_reason = wrapper->discovery_reason;
        return false;
    }

    if (!wrapper->prepared.ready()) {
        phase41->layout_mapping_valid = false;
        phase41->mapping_reason = phase42_eligibility_reason(
            wrapper->prepared.eligibility.status);
        return false;
    }

    long cursor_x = 0;
    long cursor_y = 0;
    if (!get_cursor_position(cursor_x, cursor_y)) {
        phase41->layout_mapping_valid = false;
        phase41->mapping_reason = "Windows cursor position is unavailable";
        return false;
    }

    const auto resolved = arzoom::presentation_screen_resolve_active_mapping(
        wrapper->prepared,
        static_cast<std::int64_t>(cursor_x),
        static_cast<std::int64_t>(cursor_y));
    if (!resolved.active() ||
        resolved.active_index >= wrapper->discovered_candidates.size()) {
        phase41->layout_mapping_valid = false;
        phase41->mapping_reason = phase42_resolve_reason(resolved.status);
        return false;
    }

    const SceneDisplayCandidateSnapshot &candidate =
        wrapper->discovered_candidates[resolved.active_index];
    if (!candidate.ready()) {
        phase41->layout_mapping_valid = false;
        phase41->mapping_reason = candidate.reason.empty()
                                      ? "active Presentation Screen candidate is unavailable"
                                      : candidate.reason;
        return false;
    }

    phase41->mapping = candidate.mapping;
    phase41->physical_monitor = candidate.physical_monitor;
    phase41->mapped_monitor = candidate.mapped_monitor;
    phase41->layout_mapping_valid = true;
    phase41->mapping_reason.clear();
    phase41->mapping_warning_logged = false;
    if (phase41->phase4)
        phase41->phase4->mapping_warning_logged = false;

    phase42_active_apply_native_cursor_warning(phase41, candidate);
    return true;
}

void phase42_active_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    if (!wrapper || !wrapper->settings)
        return;

    Phase41Filter *phase41 = phase42_active_phase41(wrapper);
    ArZoomFilter *phase1 = phase1_from_phase41(phase41);
    obs_source_t *scene_source = nullptr;
    if (!phase41 || !is_managed_scene_camera(phase1, &scene_source)) {
        phase42_settings_tick(wrapper->settings, seconds);
        return;
    }

    wrapper->discovery_refresh_elapsed += std::clamp(seconds, 0.0f, 0.10f);
    if (wrapper->discovery_refresh_elapsed >= kPhase42ActiveRefreshSeconds) {
        wrapper->discovery_refresh_elapsed = 0.0f;
        phase42_active_refresh_candidates(wrapper, scene_source);
    }

    phase42_active_select_mapping(wrapper, phase41);

    /* P4.2 owns structural discovery now. Keep the inherited P4.1 refresh
     * below threshold so phase41_tick only installs the chosen mapping and the
     * existing camera/click/cursor/Spotlight pipeline consumes it. */
    phase41->mapping_refresh_elapsed = 0.0f;
    phase42_settings_tick(wrapper->settings, seconds);
}

void phase42_active_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    if (!wrapper || !wrapper->settings)
        return;
    phase42_settings_update(wrapper->settings, settings);
    wrapper->discovery_refresh_elapsed = 1.0f;
}

void phase42_active_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    if (wrapper && wrapper->settings)
        phase42_settings_render(wrapper->settings, effect);
}

obs_properties_t *phase42_active_properties(void *data)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    return phase42_settings_properties(wrapper ? wrapper->settings : nullptr);
}

void phase42_active_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    if (wrapper && wrapper->settings)
        phase42_settings_deactivate(wrapper->settings);
}

void phase42_active_destroy(void *data)
{
    auto *wrapper = static_cast<Phase42ActiveFilter *>(data);
    if (!wrapper)
        return;
    phase42_settings_destroy(wrapper->settings);
    delete wrapper;
}

void *phase42_active_create(obs_data_t *settings, obs_source_t *context)
{
    auto *settings_wrapper = static_cast<Phase42SettingsFilter *>(
        phase42_settings_create(settings, context));
    if (!settings_wrapper)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase42ActiveFilter();
    if (!wrapper) {
        phase42_settings_destroy(settings_wrapper);
        return nullptr;
    }
    wrapper->settings = settings_wrapper;

    Phase41Filter *phase41 = phase42_active_phase41(wrapper);
    ArZoomFilter *phase1 = phase1_from_phase41(phase41);
    obs_source_t *scene_source = nullptr;
    if (phase41 && is_managed_scene_camera(phase1, &scene_source)) {
        phase42_active_refresh_candidates(wrapper, scene_source);
        phase42_active_select_mapping(wrapper, phase41);
        phase41->mapping_refresh_elapsed = 0.0f;
        wrapper->discovery_refresh_elapsed = 0.0f;
    }

    blog(LOG_INFO,
         "[ArZoom] P4.2 active Presentation Screen mapping adapter ready");
    return wrapper;
}

struct Phase42ActiveSourceInfoOverride {
    Phase42ActiveSourceInfoOverride()
    {
        arzoom_filter_info.create = phase42_active_create;
        arzoom_filter_info.destroy = phase42_active_destroy;
        arzoom_filter_info.video_tick = phase42_active_tick;
        arzoom_filter_info.video_render = phase42_active_render;
        arzoom_filter_info.update = phase42_active_update;
        arzoom_filter_info.get_properties = phase42_active_properties;
        arzoom_filter_info.get_defaults = phase42_settings_defaults;
        arzoom_filter_info.deactivate = phase42_active_deactivate;
    }
};

Phase42ActiveSourceInfoOverride phase42_active_source_info_override;

} // namespace
