#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("arzoom", "en-US")

extern obs_source_info arzoom_filter_info;
extern obs_source_info arzoom_camera_source_info;
bool arzoom_register_global_hotkey();
void arzoom_unregister_global_hotkey();
bool arzoom_register_presenter_hotkeys();
void arzoom_unregister_presenter_hotkeys();

bool obs_module_load(void)
{
    obs_register_source(&arzoom_filter_info);
    obs_register_source(&arzoom_camera_source_info);
    const bool toggle_ready = arzoom_register_global_hotkey();
    const bool presenter_ready = arzoom_register_presenter_hotkeys();
    blog(LOG_INFO,
         "[ArZoom] Smart Presenter Camera loaded%s",
         (toggle_ready && presenter_ready) ? "" : " (one or more hotkeys unavailable)");
    return true;
}

void obs_module_unload(void)
{
    arzoom_unregister_presenter_hotkeys();
    arzoom_unregister_global_hotkey();
    blog(LOG_INFO, "[ArZoom] Smart Presenter Camera unloaded");
}
