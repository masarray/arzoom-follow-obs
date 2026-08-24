#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("arzoom", "en-US")

/*
 * Keep the Phase 4 input-source definition in the same translation unit as
 * obs_module_load(). This removes cross-translation-unit static-initializer
 * ordering from the source-registration path: by the time module load runs,
 * arzoom_camera_source_info has deterministically been prepared.
 */
#include "arzoom-camera-source.cpp"

extern obs_source_info arzoom_filter_info;
bool arzoom_register_global_hotkey();
void arzoom_unregister_global_hotkey();
bool arzoom_register_presenter_hotkeys();
void arzoom_unregister_presenter_hotkeys();

namespace {
constexpr const char *kPhase4BuildIdentity =
    "v0.5.0-p4a-regdiag2-cc29ec0-plus";
}

bool obs_module_load(void)
{
    obs_module_t *module = obs_current_module();
    const char *binary_path = module ? obs_get_module_binary_path(module) : nullptr;
    const char *data_path = module ? obs_get_module_data_path(module) : nullptr;

    blog(LOG_INFO,
         "[ArZoom] BUILD %s",
         kPhase4BuildIdentity);
    blog(LOG_INFO,
         "[ArZoom] Loaded module binary: %s",
         (binary_path && *binary_path) ? binary_path : "<unknown>");
    blog(LOG_INFO,
         "[ArZoom] Loaded module data: %s",
         (data_path && *data_path) ? data_path : "<unknown>");

    obs_register_source(&arzoom_filter_info);

    /* Give the source a first-class camera icon in the OBS add-source UI and
     * verify registration immediately instead of assuming success. */
    arzoom_camera_source_info.icon_type = OBS_ICON_TYPE_CAMERA;
    obs_register_source(&arzoom_camera_source_info);

    const char *camera_name = obs_source_get_display_name("arzoom_camera");
    const bool camera_ready = camera_name && *camera_name;
    if (camera_ready) {
        blog(LOG_INFO,
             "[ArZoom] ArZoom Camera source registered: %s (id=arzoom_camera)",
             camera_name);
    } else {
        blog(LOG_ERROR,
             "[ArZoom] ArZoom Camera source registration FAILED (id=arzoom_camera)");
    }

    const bool toggle_ready = arzoom_register_global_hotkey();
    const bool presenter_ready = arzoom_register_presenter_hotkeys();
    blog(LOG_INFO,
         "[ArZoom] Smart Presenter Camera loaded%s%s",
         (toggle_ready && presenter_ready) ? "" : " (one or more hotkeys unavailable)",
         camera_ready ? " · ArZoom Camera ready" : " · ArZoom Camera unavailable");
    return true;
}

void obs_module_unload(void)
{
    arzoom_unregister_presenter_hotkeys();
    arzoom_unregister_global_hotkey();
    blog(LOG_INFO, "[ArZoom] Smart Presenter Camera unloaded");
}
