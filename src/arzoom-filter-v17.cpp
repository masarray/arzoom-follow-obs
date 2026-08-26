#include "arzoom-filter-v16.cpp"

/*
 * P5.1 trial-settings migration
 * -----------------------------
 * The first direct-OBS P5 build persisted `spotlight_shape=ellipse` and had no
 * continuous area-size key. Merely changing get_defaults() would therefore
 * leave existing trial users on Ellipse forever. Absence of the new area key
 * is a deterministic legacy marker: migrate that instance once to the new
 * Circle + 100% baseline, then normal user settings take ownership.
 */
namespace {

bool phase52_migrate_trial_settings(obs_data_t *settings)
{
    if (!settings ||
        obs_data_has_user_value(settings, SETTING_SPOTLIGHT_AREA_SCALE)) {
        return false;
    }

    obs_data_set_string(settings, SETTING_SPOTLIGHT_SHAPE,
                        SPOTLIGHT_SHAPE_CIRCLE);
    obs_data_set_int(settings, SETTING_SPOTLIGHT_AREA_SCALE, 100);
    blog(LOG_INFO,
         "[ArZoom] P5.1 migrated trial Spotlight settings to Circle / 100%% area");
    return true;
}

void phase52_update(void *data, obs_data_t *settings)
{
    phase52_migrate_trial_settings(settings);
    phase51_update(data, settings);
}

void *phase52_create(obs_data_t *settings, obs_source_t *context)
{
    /* Migrate before constructing the wrapper so its first visible frame never
     * inherits the trial Ellipse value. phase51_create() remains the sole owner
     * of allocation/registration; P5.2 only normalizes settings first. */
    phase52_migrate_trial_settings(settings);
    return phase51_create(settings, context);
}

struct Phase52SourceInfoOverride {
    Phase52SourceInfoOverride()
    {
        arzoom_filter_info.create = phase52_create;
        arzoom_filter_info.update = phase52_update;
        arzoom_filter_info.get_defaults = phase51_defaults;
    }
};

Phase52SourceInfoOverride phase52_source_info_override;

} // namespace
