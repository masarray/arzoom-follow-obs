#include "arzoom-filter-v24.cpp"
#include "arzoom-presentation-screen-settings.hpp"

#include <string>
#include <vector>

/*
 * P4.2 Slice 4 — Presentation Screens settings model
 * ===================================================
 *
 * This wrapper persists Presentation Screen eligibility separately from active
 * camera ownership. Source UUIDs are the only durable identity. The selected
 * UUID list is read from OBS settings during create/update and remains inert
 * until the later active-mapping adapter consumes it.
 *
 * This slice deliberately does not switch the camera between displays, does
 * not rebuild Properties, and does not write settings per frame.
 */
namespace {

struct Phase42SettingsFilter {
    Phase24Filter *phase24 = nullptr;
    arzoom::PresentationScreenSelectionSettings presentation_screens{};
};

arzoom::PresentationScreenSelectionWire phase42_read_settings_wire(
    obs_data_t *settings)
{
    arzoom::PresentationScreenSelectionWire wire;
    if (!settings)
        return wire;

    const long long schema = obs_data_get_int(
        settings, arzoom::kPresentationScreenSettingsSchemaKey);
    wire.schema_version = schema > 0
                              ? static_cast<int>(schema)
                              : arzoom::kPresentationScreenSettingsSchemaVersion;
    wire.persisted = obs_data_has_user_value(
        settings, arzoom::kPresentationScreenSettingsArrayKey);
    if (!wire.persisted)
        return wire;

    obs_data_array_t *array = obs_data_get_array(
        settings, arzoom::kPresentationScreenSettingsArrayKey);
    if (!array)
        return wire;

    const size_t count = obs_data_array_count(array);
    wire.source_uuids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        obs_data_t *entry = obs_data_array_item(array, i);
        if (!entry)
            continue;
        const char *uuid = obs_data_get_string(
            entry, arzoom::kPresentationScreenSettingsUuidKey);
        if (uuid && *uuid)
            wire.source_uuids.emplace_back(uuid);
        obs_data_release(entry);
    }
    obs_data_array_release(array);
    return wire;
}

arzoom::PresentationScreenSelectionSettings phase42_read_settings(
    obs_data_t *settings)
{
    return arzoom::presentation_screen_decode_selection(
        phase42_read_settings_wire(settings));
}

void phase42_settings_defaults(obs_data_t *settings)
{
    phase23_defaults(settings);
    if (!settings)
        return;

    obs_data_set_default_int(
        settings,
        arzoom::kPresentationScreenSettingsSchemaKey,
        arzoom::kPresentationScreenSettingsSchemaVersion);

    obs_data_array_t *empty = obs_data_array_create();
    if (empty) {
        obs_data_set_default_array(
            settings, arzoom::kPresentationScreenSettingsArrayKey, empty);
        obs_data_array_release(empty);
    }
}

void phase42_settings_tick(void *data, float seconds)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    if (wrapper && wrapper->phase24)
        phase24_tick(wrapper->phase24, seconds);
}

void phase42_settings_update(void *data, obs_data_t *settings)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    if (!wrapper || !wrapper->phase24)
        return;

    /* Preserve P5.6 update semantics exactly; Slice 4 only snapshots the
     * persisted UUID-selection model after the inherited update completes. */
    phase24_update(wrapper->phase24, settings);
    wrapper->presentation_screens = phase42_read_settings(settings);
}

void phase42_settings_render(void *data, gs_effect_t *effect)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    if (wrapper && wrapper->phase24)
        phase24_render(wrapper->phase24, effect);
}

obs_properties_t *phase42_settings_properties(void *data)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    return phase24_properties(wrapper ? wrapper->phase24 : nullptr);
}

void phase42_settings_deactivate(void *data)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    if (wrapper && wrapper->phase24)
        phase24_deactivate(wrapper->phase24);
}

void phase42_settings_destroy(void *data)
{
    auto *wrapper = static_cast<Phase42SettingsFilter *>(data);
    if (!wrapper)
        return;
    phase24_destroy(wrapper->phase24);
    delete wrapper;
}

void *phase42_settings_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase24 = static_cast<Phase24Filter *>(
        phase24_create(settings, context));
    if (!phase24)
        return nullptr;

    auto *wrapper = new (std::nothrow) Phase42SettingsFilter();
    if (!wrapper) {
        phase24_destroy(phase24);
        return nullptr;
    }

    wrapper->phase24 = phase24;
    wrapper->presentation_screens = phase42_read_settings(settings);

    blog(LOG_INFO,
         "[ArZoom] P4.2 Presentation Screens settings model ready (%zu persisted UUID%s)",
         wrapper->presentation_screens.selected_source_uuids.size(),
         wrapper->presentation_screens.selected_source_uuids.size() == 1 ? "" : "s");
    return wrapper;
}

struct Phase42SettingsSourceInfoOverride {
    Phase42SettingsSourceInfoOverride()
    {
        arzoom_filter_info.create = phase42_settings_create;
        arzoom_filter_info.destroy = phase42_settings_destroy;
        arzoom_filter_info.video_tick = phase42_settings_tick;
        arzoom_filter_info.video_render = phase42_settings_render;
        arzoom_filter_info.update = phase42_settings_update;
        arzoom_filter_info.get_properties = phase42_settings_properties;
        arzoom_filter_info.get_defaults = phase42_settings_defaults;
        arzoom_filter_info.deactivate = phase42_settings_deactivate;
    }
};

Phase42SettingsSourceInfoOverride phase42_settings_source_info_override;

} // namespace
