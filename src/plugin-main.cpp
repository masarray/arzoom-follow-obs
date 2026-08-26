#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("arzoom", "en-US")

extern obs_source_info arzoom_filter_info;
bool arzoom_register_global_hotkey();
void arzoom_unregister_global_hotkey();
bool arzoom_register_presenter_hotkeys();
void arzoom_unregister_presenter_hotkeys();
bool arzoom_register_spotlight_hotkeys();
void arzoom_unregister_spotlight_hotkeys();
bool arzoom_register_scene_camera_tools();

namespace {
constexpr const char *kPhase4BuildIdentity =
    "v0.7.0-p5-spotlight-stable";
}

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

    /* Phase 4 deliberately reuses the already accepted ArZoom effect filter.
     * An OBS scene is itself a video source, so the frontend Scene Camera
     * manager attaches this same filter directly to the scene source instead
     * of registering a second input source or re-rendering the scene graph. */
    obs_register_source(&arzoom_filter_info);

    const char *filter_name = obs_source_get_display_name("arzoom_filter");
    const bool filter_ready = filter_name && *filter_name;
    if (!filter_ready) {
        blog(LOG_ERROR,
             "[ArZoom] arzoom_filter registration FAILED; Scene Camera cannot start");
    }

    const bool toggle_ready = arzoom_register_global_hotkey();
    const bool presenter_ready = arzoom_register_presenter_hotkeys();
    const bool spotlight_ready = arzoom_register_spotlight_hotkeys();
    const bool scene_camera_ready =
        filter_ready && arzoom_register_scene_camera_tools();

    blog(LOG_INFO,
         "[ArZoom] Smart Presenter Camera loaded%s%s",
         (toggle_ready && presenter_ready && spotlight_ready)
             ? "" : " (one or more hotkeys unavailable)",
         scene_camera_ready ? " · Scene Camera tools ready"
                            : " · Scene Camera unavailable");
    return true;
}

void obs_module_unload(void)
{
    arzoom_unregister_spotlight_hotkeys();
    arzoom_unregister_presenter_hotkeys();
    arzoom_unregister_global_hotkey();
    blog(LOG_INFO, "[ArZoom] Smart Presenter Camera unloaded");
}
