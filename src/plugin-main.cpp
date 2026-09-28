#include <obs-module.h>
#include <obs-frontend-api.h>

#include "arzoom-filter-boundary.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("arzoom", "en-US")

extern obs_source_info arzoom_filter_info;
extern obs_source_info arzoom_multi_filter_info;
bool arzoom_register_global_hotkey();
void arzoom_unregister_global_hotkey();
bool arzoom_register_presenter_hotkeys();
void arzoom_unregister_presenter_hotkeys();
bool arzoom_register_spotlight_hotkeys();
void arzoom_unregister_spotlight_hotkeys();
bool arzoom_register_scene_camera_tools();

namespace {
constexpr const char *kPhase4BuildIdentity =
    "v0.7.0-arzoom-multi-m0";

const char *single_filter_name(void *)
{
    return arzoom::kSingleFilterDisplayName.data();
}

bool registration_identity_is_expected(const obs_source_info &info,
                                       arzoom::CameraFilterKind expected)
{
    return arzoom::camera_filter_kind(info.id) == expected;
}
} // namespace

bool obs_module_load(void)
{
    obs_module_t *module = obs_current_module();
    const char *binary_path = module ? obs_get_module_binary_path(module) : nullptr;
    const char *data_path = module ? obs_get_module_data_path(module) : nullptr;

    blog(LOG_INFO, "[ArZoom] BUILD %s", kPhase4BuildIdentity);
    blog(LOG_INFO,
         "[ArZoom] Loaded module binary: %s",
         (binary_path && *binary_path) ? binary_path : "<unknown>");
    blog(LOG_INFO,
         "[ArZoom] Loaded module data: %s",
         (data_path && *data_path) ? data_path : "<unknown>");

    /* M0 preserves the legacy internal ID exactly.  Existing OBS projects
     * continue to resolve `arzoom_filter`; only its user-facing type name is
     * changed to ArZoom Single. */
    if (!registration_identity_is_expected(
            arzoom_filter_info, arzoom::CameraFilterKind::Single)) {
        blog(LOG_ERROR,
             "[ArZoom] Stable filter internal ID changed unexpectedly; refusing to register an incompatible Single boundary");
        return false;
    }
    if (!registration_identity_is_expected(
            arzoom_multi_filter_info, arzoom::CameraFilterKind::Multi)) {
        blog(LOG_ERROR,
             "[ArZoom] ArZoom Multi internal ID is invalid; M0 registration aborted");
        return false;
    }

    arzoom_filter_info.get_name = single_filter_name;
    obs_register_source(&arzoom_filter_info);
    obs_register_source(&arzoom_multi_filter_info);

    const char *single_name =
        obs_source_get_display_name(arzoom::kSingleFilterId.data());
    const char *multi_name =
        obs_source_get_display_name(arzoom::kMultiFilterId.data());
    const bool filter_ready = single_name && *single_name;
    const bool multi_ready = multi_name && *multi_name;

    if (!filter_ready) {
        blog(LOG_ERROR,
             "[ArZoom] arzoom_filter registration FAILED; Scene Camera cannot start");
    }
    if (!multi_ready) {
        blog(LOG_ERROR,
             "[ArZoom Multi] arzoom_filter_multi registration FAILED; Multi is unavailable");
    }

    blog(LOG_INFO,
         "[ArZoom] M0 filter boundary: %s='%s' · %s='%s'",
         arzoom::kSingleFilterId.data(),
         filter_ready ? single_name : "<unavailable>",
         arzoom::kMultiFilterId.data(),
         multi_ready ? multi_name : "<unavailable>");

    const bool toggle_ready = arzoom_register_global_hotkey();
    const bool presenter_ready = arzoom_register_presenter_hotkeys();
    const bool spotlight_ready = arzoom_register_spotlight_hotkeys();
    const bool scene_camera_ready =
        filter_ready && arzoom_register_scene_camera_tools();

    blog(LOG_INFO,
         "[ArZoom] Smart Presenter Camera loaded%s%s%s",
         (toggle_ready && presenter_ready && spotlight_ready)
             ? "" : " (one or more hotkeys unavailable)",
         scene_camera_ready ? " · Scene Camera tools ready"
                            : " · Scene Camera unavailable",
         multi_ready ? " · ArZoom Multi M0 registered"
                     : " · ArZoom Multi unavailable");
    return true;
}

void obs_module_unload(void)
{
    arzoom_unregister_spotlight_hotkeys();
    arzoom_unregister_presenter_hotkeys();
    arzoom_unregister_global_hotkey();
    blog(LOG_INFO, "[ArZoom] Smart Presenter Camera unloaded");
}
