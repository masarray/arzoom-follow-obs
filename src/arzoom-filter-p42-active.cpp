#include "arzoom-filter-p42-settings.cpp"
#include "arzoom-presentation-screen-active-mapping.hpp"

#include <algorithm>
#include <cstdint>
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
        return;
    }

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
