#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("arzoom", "en-US")

extern obs_source_info arzoom_filter_info;
bool arzoom_register_global_hotkey();
void arzoom_unregister_global_hotkey();

bool obs_module_load(void)
{
    obs_register_source(&arzoom_filter_info);
    const bool hotkey_ready = arzoom_register_global_hotkey();
    blog(LOG_INFO,
         "[ArZoom] Smart Mouse Zoom loaded%s",
         hotkey_ready ? "" : " (hotkey unavailable)");
    return true;
}

void obs_module_unload(void)
{
    arzoom_unregister_global_hotkey();
    blog(LOG_INFO, "[ArZoom] Smart Mouse Zoom unloaded");
}
