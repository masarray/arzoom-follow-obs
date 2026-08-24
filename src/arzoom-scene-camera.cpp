#include "arzoom-scene-camera-core.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <cstring>
#include <string>
#include <unordered_set>

namespace {

constexpr const char *kArZoomEnabledSetting = "enabled";

bool managed_marker(obs_source_t *filter)
{
    if (!filter)
        return false;
    obs_data_t *settings = obs_source_get_settings(filter);
    if (!settings)
        return false;
    const bool managed = obs_data_get_bool(
        settings, arzoom::kSceneCameraManagedSetting.data());
    obs_data_release(settings);
    return managed;
}

bool arzoom_filter_effectively_enabled(obs_source_t *filter)
{
    if (!filter || !obs_source_enabled(filter))
        return false;
    obs_data_t *settings = obs_source_get_settings(filter);
    if (!settings)
        return true;
    const bool enabled = obs_data_get_bool(settings, kArZoomEnabledSetting);
    obs_data_release(settings);
    return enabled;
}

void sync_managed_filter_enabled(obs_source_t *filter, bool enabled)
{
    if (!filter)
        return;

    obs_data_t *settings = obs_source_get_settings(filter);
    if (settings) {
        obs_data_set_bool(settings,
                          arzoom::kSceneCameraManagedSetting.data(), true);
        obs_data_set_bool(settings, kArZoomEnabledSetting, enabled);
        obs_source_update(filter, settings);
        obs_data_release(settings);
    }
    obs_source_set_enabled(filter, enabled);
}

struct FilterFindContext {
    obs_source_t *found = nullptr;
};

void find_managed_filter_cb(obs_source_t *, obs_source_t *filter, void *param)
{
    auto *ctx = static_cast<FilterFindContext *>(param);
    if (!ctx || ctx->found || !filter)
        return;

    const char *id = obs_source_get_id(filter);
    const char *name = obs_source_get_name(filter);
    if (!id || !name)
        return;

    if (arzoom::scene_camera_filter_matches(
            id, name, managed_marker(filter))) {
        ctx->found = obs_source_get_ref(filter);
    }
}

obs_source_t *find_managed_scene_camera(obs_source_t *scene_source)
{
    if (!scene_source)
        return nullptr;

    FilterFindContext ctx;
    obs_source_enum_filters(scene_source, find_managed_filter_cb, &ctx);
    return ctx.found;
}

struct CountFilterContext {
    size_t count = 0;
};

void count_arzoom_filter_cb(obs_source_t *, obs_source_t *filter, void *param)
{
    auto *ctx = static_cast<CountFilterContext *>(param);
    if (!ctx || !filter)
        return;

    const char *id = obs_source_get_id(filter);
    if (id && std::strcmp(id, "arzoom_filter") == 0 &&
        arzoom_filter_effectively_enabled(filter)) {
        ++ctx->count;
    }
}

void count_nested_arzoom_filters(obs_scene_t *scene,
                                 std::unordered_set<const void *> &visited,
                                 size_t depth,
                                 size_t &count)
{
    if (!scene || depth > 12 || !visited.insert(scene).second)
        return;

    struct EnumContext {
        std::unordered_set<const void *> *visited = nullptr;
        size_t depth = 0;
        size_t *count = nullptr;
    } ctx{&visited, depth, &count};

    obs_scene_enum_items(
        scene,
        [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
            auto *ctx = static_cast<EnumContext *>(param);
            if (!ctx || !ctx->visited || !ctx->count || !item)
                return true;

            obs_source_t *source = obs_sceneitem_get_source(item);
            if (!source)
                return true;

            CountFilterContext filter_count;
            obs_source_enum_filters(source, count_arzoom_filter_cb,
                                    &filter_count);
            *ctx->count += filter_count.count;

            if (obs_scene_t *nested = obs_scene_from_source(source)) {
                count_nested_arzoom_filters(
                    nested, *ctx->visited, ctx->depth + 1, *ctx->count);
            }
            return true;
        },
        &ctx);
}

size_t enabled_nested_arzoom_filter_count(obs_source_t *scene_source)
{
    obs_scene_t *scene = scene_source ? obs_scene_from_source(scene_source)
                                      : nullptr;
    if (!scene)
        return 0;

    std::unordered_set<const void *> visited;
    size_t count = 0;
    count_nested_arzoom_filters(scene, visited, 0, count);
    return count;
}

void warn_double_zoom_if_needed(obs_source_t *scene_source)
{
    const size_t nested = enabled_nested_arzoom_filter_count(scene_source);
    if (nested == 0)
        return;
    blog(LOG_WARNING,
         "[ArZoom] Scene Camera detected %zu enabled nested/per-source ArZoom filter(s). "
         "Disable those filters in this scene to avoid applying zoom twice.",
         nested);
}

obs_source_t *create_managed_scene_camera(obs_source_t *scene_source)
{
    if (!scene_source)
        return nullptr;

    obs_data_t *settings = obs_data_create();
    obs_data_set_bool(settings,
                      arzoom::kSceneCameraManagedSetting.data(), true);
    obs_data_set_bool(settings, kArZoomEnabledSetting, true);
    obs_source_t *filter = obs_source_create(
        "arzoom_filter", "ArZoom Camera", settings, nullptr);
    obs_data_release(settings);

    if (!filter) {
        blog(LOG_ERROR,
             "[ArZoom] Scene Camera could not create the arzoom_filter instance");
        return nullptr;
    }

    obs_source_filter_add(scene_source, filter);
    sync_managed_filter_enabled(filter, true);
    obs_frontend_save();

    const char *scene_name = obs_source_get_name(scene_source);
    blog(LOG_INFO,
         "[ArZoom] Scene Camera attached to scene '%s' using the native OBS filter chain",
         scene_name ? scene_name : "<unnamed>");
    warn_double_zoom_if_needed(scene_source);
    return filter;
}

obs_source_t *ensure_managed_scene_camera(obs_source_t *scene_source)
{
    if (!scene_source)
        return nullptr;
    if (obs_source_t *existing = find_managed_scene_camera(scene_source))
        return existing;
    return create_managed_scene_camera(scene_source);
}

void toggle_scene_camera_cb(void *)
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source) {
        blog(LOG_WARNING,
             "[ArZoom] Scene Camera toggle ignored because OBS has no current scene");
        return;
    }

    obs_source_t *filter = find_managed_scene_camera(scene_source);
    const bool exists = filter != nullptr;
    const bool enabled = filter ? arzoom_filter_effectively_enabled(filter)
                                : false;
    const auto action = arzoom::scene_camera_toggle_action(exists, enabled);

    if (action == arzoom::SceneCameraToggleAction::CreateEnabled) {
        filter = create_managed_scene_camera(scene_source);
    } else if (filter) {
        const bool next_enabled =
            action == arzoom::SceneCameraToggleAction::EnableExisting;
        sync_managed_filter_enabled(filter, next_enabled);
        obs_frontend_save();
        blog(LOG_INFO,
             "[ArZoom] Scene Camera %s for scene '%s'",
             next_enabled ? "enabled" : "disabled",
             obs_source_get_name(scene_source));
        if (next_enabled)
            warn_double_zoom_if_needed(scene_source);
    }

    if (filter)
        obs_source_release(filter);
    obs_source_release(scene_source);
}

void configure_scene_camera_cb(void *)
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source) {
        blog(LOG_WARNING,
             "[ArZoom] Scene Camera configure ignored because OBS has no current scene");
        return;
    }

    obs_source_t *filter = ensure_managed_scene_camera(scene_source);
    if (filter) {
        sync_managed_filter_enabled(filter, true);
        obs_source_release(filter);
        obs_frontend_save();
        warn_double_zoom_if_needed(scene_source);
        obs_frontend_open_source_filters(scene_source);
    }

    obs_source_release(scene_source);
}

} // namespace

bool arzoom_register_scene_camera_tools()
{
    obs_frontend_add_tools_menu_item(
        obs_module_text("ArZoom.SceneCamera.Toggle"),
        toggle_scene_camera_cb, nullptr);
    obs_frontend_add_tools_menu_item(
        obs_module_text("ArZoom.SceneCamera.Configure"),
        configure_scene_camera_cb, nullptr);

    blog(LOG_INFO,
         "[ArZoom] Phase 4 Scene Camera tools ready (native scene-filter architecture)");
    return true;
}
