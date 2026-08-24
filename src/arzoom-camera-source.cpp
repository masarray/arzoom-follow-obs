#include "arzoom-camera-source-core.hpp"

#include <obs-module.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

namespace {

constexpr const char *kCameraSourceId = "arzoom_camera";
constexpr const char *kTargetUuidSetting = "arzoom_camera_target_uuid";

struct ArZoomCameraSource {
    obs_source_t *context = nullptr;

    std::mutex target_mutex;
    obs_weak_source_t *target_weak = nullptr;
    std::string target_uuid;

    gs_texrender_t *texrender = nullptr;
    arzoom::CameraRenderSize render_size{};

    std::atomic<uint32_t> output_width{1920};
    std::atomic<uint32_t> output_height{1080};
    std::atomic<uint64_t> recursion_rejects{0};
};

const char *camera_get_name(void *)
{
    return "ArZoom Camera";
}

obs_source_t *camera_acquire_target(ArZoomCameraSource *camera)
{
    if (!camera)
        return nullptr;

    obs_weak_source_t *weak = nullptr;
    {
        std::lock_guard<std::mutex> lock(camera->target_mutex);
        weak = camera->target_weak;
        if (weak)
            obs_weak_source_addref(weak);
    }

    if (!weak)
        return nullptr;

    obs_source_t *target = obs_weak_source_get_source(weak);
    obs_weak_source_release(weak);
    return target;
}

void camera_replace_target(ArZoomCameraSource *camera,
                           obs_weak_source_t *new_weak,
                           std::string new_uuid)
{
    if (!camera)
        return;

    obs_weak_source_t *old_weak = nullptr;
    {
        std::lock_guard<std::mutex> lock(camera->target_mutex);
        old_weak = camera->target_weak;
        camera->target_weak = new_weak;
        camera->target_uuid = std::move(new_uuid);
    }

    if (old_weak)
        obs_weak_source_release(old_weak);
}

void camera_update(void *data, obs_data_t *settings)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    if (!camera || !settings)
        return;

    const char *uuid_value = obs_data_get_string(settings, kTargetUuidSetting);
    const std::string requested_uuid = uuid_value ? uuid_value : "";

    {
        std::lock_guard<std::mutex> lock(camera->target_mutex);
        if (requested_uuid == camera->target_uuid)
            return;
    }

    obs_source_t *target = nullptr;
    obs_weak_source_t *new_weak = nullptr;

    if (!requested_uuid.empty())
        target = obs_get_source_by_uuid(requested_uuid.c_str());

    if (target && target != camera->context)
        new_weak = obs_source_get_weak_source(target);

    if (target)
        obs_source_release(target);

    camera_replace_target(camera, new_weak,
                          new_weak ? requested_uuid : std::string{});
}

void camera_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, kTargetUuidSetting, "");
}

struct CameraTargetListContext {
    obs_property_t *property = nullptr;
    obs_source_t *self = nullptr;
};

bool camera_enum_source(void *param, obs_source_t *source)
{
    auto *ctx = static_cast<CameraTargetListContext *>(param);
    if (!ctx || !ctx->property || !source || source == ctx->self)
        return true;

    const uint32_t flags = obs_source_get_output_flags(source);
    if ((flags & OBS_SOURCE_VIDEO) == 0)
        return true;

    const char *name = obs_source_get_name(source);
    const char *uuid = obs_source_get_uuid(source);
    if (!name || !*name || !uuid || !*uuid)
        return true;

    obs_property_list_add_string(ctx->property, name, uuid);
    return true;
}

obs_properties_t *camera_properties(void *data)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    obs_properties_t *props = obs_properties_create();

    obs_property_t *target = obs_properties_add_list(
        props, kTargetUuidSetting, "Scene / source to frame",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(target, "None", "");

    CameraTargetListContext list_context{
        target, camera ? camera->context : nullptr};
    obs_enum_sources(camera_enum_source, &list_context);

    obs_property_t *info = obs_properties_add_text(
        props, "arzoom_camera_phase4_info",
        "Phase 4 foundation: ArZoom Camera renders the selected scene/source "
        "through an off-screen GPU target without modifying scene-item "
        "transforms. Smart Camera motion integration follows in the next "
        "Phase 4 step.",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    return props;
}

void camera_destroy_texrender(ArZoomCameraSource *camera)
{
    if (!camera || !camera->texrender)
        return;
    gs_texrender_destroy(camera->texrender);
    camera->texrender = nullptr;
    camera->render_size = {};
}

bool camera_ensure_texrender(ArZoomCameraSource *camera,
                             arzoom::CameraRenderSize requested)
{
    if (!camera)
        return false;

    const auto action = arzoom::camera_render_target_action(
        camera->render_size, requested);
    if (action == arzoom::CameraRenderTargetAction::Invalid)
        return false;

    if (action == arzoom::CameraRenderTargetAction::Reuse && camera->texrender)
        return true;

    camera_destroy_texrender(camera);
    camera->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    if (!camera->texrender)
        return false;

    camera->render_size = requested;
    return true;
}

void camera_draw_texture(ArZoomCameraSource *camera)
{
    if (!camera || !camera->texrender)
        return;

    gs_texture_t *texture = gs_texrender_get_texture(camera->texrender);
    if (!texture)
        return;

    gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    if (!effect)
        return;

    gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
    if (!image)
        return;

    gs_effect_set_texture(image, texture);
    while (gs_effect_loop(effect, "Draw"))
        gs_draw_sprite(texture, 0, camera->render_size.width,
                       camera->render_size.height);
}

void camera_render(void *data, gs_effect_t *)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    if (!camera)
        return;

    arzoom::CameraRenderRecursionGuard recursion(camera->context);
    if (!recursion) {
        camera->recursion_rejects.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    obs_source_t *target = camera_acquire_target(camera);
    if (!target || target == camera->context) {
        if (target)
            obs_source_release(target);
        return;
    }

    const arzoom::CameraRenderSize requested{
        obs_source_get_width(target), obs_source_get_height(target)};
    if (!arzoom::camera_render_size_valid(requested)) {
        obs_source_release(target);
        return;
    }

    camera->output_width.store(requested.width, std::memory_order_release);
    camera->output_height.store(requested.height, std::memory_order_release);

    if (!camera_ensure_texrender(camera, requested)) {
        obs_source_release(target);
        return;
    }

    gs_texrender_reset(camera->texrender);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    if (gs_texrender_begin(camera->texrender, requested.width,
                           requested.height)) {
        struct vec4 clear_color;
        vec4_zero(&clear_color);
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, static_cast<float>(requested.width),
                 0.0f, static_cast<float>(requested.height),
                 -100.0f, 100.0f);
        obs_source_video_render(target);
        gs_texrender_end(camera->texrender);
    }

    gs_blend_state_pop();
    obs_source_release(target);

    camera_draw_texture(camera);
}

uint32_t camera_get_width(void *data)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    return camera ? camera->output_width.load(std::memory_order_acquire) : 1;
}

uint32_t camera_get_height(void *data)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    return camera ? camera->output_height.load(std::memory_order_acquire) : 1;
}

void camera_enum_active_sources(void *data,
                                obs_source_enum_proc_t enum_callback,
                                void *param)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    if (!camera || !enum_callback)
        return;

    obs_source_t *target = camera_acquire_target(camera);
    if (!target)
        return;

    enum_callback(camera->context, target, param);
    obs_source_release(target);
}

void camera_destroy(void *data)
{
    auto *camera = static_cast<ArZoomCameraSource *>(data);
    if (!camera)
        return;

    camera_replace_target(camera, nullptr, {});

    obs_enter_graphics();
    camera_destroy_texrender(camera);
    obs_leave_graphics();

    delete camera;
}

void *camera_create(obs_data_t *settings, obs_source_t *context)
{
    auto *camera = new (std::nothrow) ArZoomCameraSource();
    if (!camera)
        return nullptr;

    camera->context = context;

    obs_video_info video_info{};
    if (obs_get_video_info(&video_info) && video_info.base_width > 0 &&
        video_info.base_height > 0) {
        camera->output_width.store(video_info.base_width,
                                   std::memory_order_relaxed);
        camera->output_height.store(video_info.base_height,
                                    std::memory_order_relaxed);
    }

    camera_update(camera, settings);
    blog(LOG_INFO, "[ArZoom] Phase 4 ArZoom Camera source created");
    return camera;
}

} // namespace

obs_source_info arzoom_camera_source_info = {
    .id = kCameraSourceId,
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
                    OBS_SOURCE_COMPOSITE,
    .get_name = camera_get_name,
    .create = camera_create,
    .destroy = camera_destroy,
    .get_width = camera_get_width,
    .get_height = camera_get_height,
    .get_defaults = camera_defaults,
    .get_properties = camera_properties,
    .update = camera_update,
    .video_render = camera_render,
    .enum_active_sources = camera_enum_active_sources,
};
