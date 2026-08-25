#include "arzoom-filter-v14.cpp"
#include "arzoom-spotlight.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

#define SETTING_SPOTLIGHT_ENABLED "spotlight_enabled"
#define SETTING_SPOTLIGHT_MODE "spotlight_mode"
#define SETTING_SPOTLIGHT_SIZE "spotlight_size"
#define SETTING_SPOTLIGHT_DIM "spotlight_dim"
#define SETTING_SPOTLIGHT_SHAPE "spotlight_shape"
#define SETTING_SPOTLIGHT_FEATHER "spotlight_feather"

#define SPOTLIGHT_MODE_SMART "smart"
#define SPOTLIGHT_MODE_CURSOR "cursor"
#define SPOTLIGHT_MODE_CLICK "click"
#define SPOTLIGHT_SIZE_COMPACT "compact"
#define SPOTLIGHT_SIZE_BALANCED "balanced"
#define SPOTLIGHT_SIZE_WIDE "wide"
#define SPOTLIGHT_SHAPE_ELLIPSE "ellipse"
#define SPOTLIGHT_SHAPE_ROUNDED_RECT "rounded_rect"

/*
 * P5 Spotlight runtime
 * ====================
 *
 * This wrapper deliberately sits above the accepted P4.1 v14 runtime. When
 * Spotlight is inactive it delegates rendering directly to v14, preserving the
 * existing camera/click/cursor path. When Spotlight is active, it uses the same
 * already-composed scene texture and the same effect pass, adding only analytic
 * mask uniforms. No scene-item mutation, helper source, frame readback, second
 * render graph, or image analysis is introduced.
 *
 * Spotlight is a presentation-effect consumer. It can read mapped pointer,
 * click and camera state, but it never writes camera intent or motion state.
 */
namespace {

struct Phase5Filter {
    Phase41Filter *phase41 = nullptr;

    std::atomic<bool> spotlight_enabled{false};
    std::atomic<int> spotlight_mode{
        static_cast<int>(arzoom::SpotlightMode::SmartFocus)};
    std::atomic<int> spotlight_size{
        static_cast<int>(arzoom::SpotlightSize::Balanced)};
    std::atomic<float> spotlight_dim{0.38f};
    std::atomic<int> spotlight_shape{
        static_cast<int>(arzoom::SpotlightShape::Ellipse)};
    std::atomic<float> spotlight_feather_px{92.0f};
    std::atomic<bool> preview_requested{false};
    std::atomic<bool> runtime_reset_requested{false};

    /* Video-tick owns validity/history. Render sees only compact atomics. */
    bool center_valid = false;
    float center_output_x = 0.5f;
    float center_output_y = 0.45f;
    bool click_anchor_valid = false;
    arzoom::Vec2 click_anchor_content{0.5f, 0.5f};
    uint32_t last_click_generation = 0;

    std::atomic<bool> runtime_active{false};
    std::atomic<float> render_center_x{0.5f};
    std::atomic<float> render_center_y{0.45f};

    gs_eparam_t *enabled_param = nullptr;
    gs_eparam_t *center_param = nullptr;
    gs_eparam_t *half_size_param = nullptr;
    gs_eparam_t *feather_param = nullptr;
    gs_eparam_t *dim_param = nullptr;
    gs_eparam_t *shape_param = nullptr;
    gs_eparam_t *corner_param = nullptr;
    bool shader_ready = false;
};

arzoom::SpotlightMode parse_spotlight_mode(const char *value)
{
    if (value && std::strcmp(value, SPOTLIGHT_MODE_CURSOR) == 0)
        return arzoom::SpotlightMode::Cursor;
    if (value && std::strcmp(value, SPOTLIGHT_MODE_CLICK) == 0)
        return arzoom::SpotlightMode::Click;
    return arzoom::SpotlightMode::SmartFocus;
}

arzoom::SpotlightSize parse_spotlight_size(const char *value)
{
    if (value && std::strcmp(value, SPOTLIGHT_SIZE_COMPACT) == 0)
        return arzoom::SpotlightSize::Compact;
    if (value && std::strcmp(value, SPOTLIGHT_SIZE_WIDE) == 0)
        return arzoom::SpotlightSize::Wide;
    return arzoom::SpotlightSize::Balanced;
}

arzoom::SpotlightShape parse_spotlight_shape(const char *value)
{
    if (value && std::strcmp(value, SPOTLIGHT_SHAPE_ROUNDED_RECT) == 0)
        return arzoom::SpotlightShape::RoundedRectangle;
    return arzoom::SpotlightShape::Ellipse;
}

Phase35Filter *phase5_phase35(Phase5Filter *filter)
{
    return filter && filter->phase41 && filter->phase41->phase4
               ? phase35_from_phase4(filter->phase41->phase4)
               : nullptr;
}

Phase2Filter *phase5_phase2(Phase5Filter *filter)
{
    Phase35Filter *phase35 = phase5_phase35(filter);
    return phase35 ? phase2_filter(phase35) : nullptr;
}

ArZoomFilter *phase5_phase1(Phase5Filter *filter)
{
    return filter && filter->phase41
               ? phase1_from_phase41(filter->phase41)
               : nullptr;
}

Phase351Filter *phase5_phase351(Phase5Filter *filter)
{
    return filter && filter->phase41
               ? phase351_from_phase41(filter->phase41)
               : nullptr;
}

float spotlight_visual_alpha(float dt, float time_constant)
{
    const float safe_dt = std::clamp(dt, 0.0f, 0.10f);
    const float tau = std::max(time_constant, 0.001f);
    return 1.0f - std::exp(-safe_dt / tau);
}

void move_spotlight_center(Phase5Filter *filter, arzoom::Vec2 target,
                           float dt, float time_constant)
{
    if (!filter)
        return;

    if (!filter->center_valid || time_constant <= 0.001f) {
        filter->center_output_x = target.x;
        filter->center_output_y = target.y;
        filter->center_valid = true;
    } else {
        const float alpha = spotlight_visual_alpha(dt, time_constant);
        filter->center_output_x +=
            (target.x - filter->center_output_x) * alpha;
        filter->center_output_y +=
            (target.y - filter->center_output_y) * alpha;
    }

    filter->render_center_x.store(filter->center_output_x,
                                  std::memory_order_release);
    filter->render_center_y.store(filter->center_output_y,
                                  std::memory_order_release);
}

bool mapped_pointer_content(ArZoomFilter *phase1,
                            arzoom::Vec2 &content_position)
{
    if (!phase1 || !phase1->monitor_valid)
        return false;

    long cursor_x = 0;
    long cursor_y = 0;
    if (!get_cursor_position(cursor_x, cursor_y) ||
        !cursor_in_monitor(phase1->monitor, cursor_x, cursor_y)) {
        return false;
    }

    content_position = cursor_normalized(
        phase1->monitor, cursor_x, cursor_y);
    return std::isfinite(content_position.x) &&
           std::isfinite(content_position.y);
}

arzoom::Vec2 content_to_live_output(ArZoomFilter *phase1,
                                    arzoom::Vec2 content_position)
{
    if (!phase1)
        return {0.5f, 0.5f};
    return arzoom::project_content_to_output(
        content_position, phase1->current_center,
        std::max(phase1->current_zoom, 1.0f));
}

void capture_latest_spotlight_click(Phase5Filter *filter)
{
    Phase2Filter *phase2 = phase5_phase2(filter);
    if (!filter || !phase2)
        return;

    const arzoom::ClickEvent *newest = nullptr;
    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i) {
        const arzoom::ClickEvent &event = phase2->clicks.slot(i);
        if (!event.active() || event.generation <= filter->last_click_generation)
            continue;
        if (!newest || event.generation > newest->generation)
            newest = &event;
    }

    if (!newest)
        return;

    filter->last_click_generation = newest->generation;
    filter->click_anchor_content = newest->content_position;
    filter->click_anchor_valid = true;
}

bool camera_is_semantically_moving(ArZoomFilter *phase1)
{
    if (!phase1)
        return false;
    const arzoom::CameraState state = phase1->camera.output().state;
    return state != arzoom::CameraState::Rest &&
           state != arzoom::CameraState::SmoothIdle;
}

void phase5_prepare_click_sampling(Phase5Filter *filter)
{
    Phase35Filter *phase35 = phase5_phase35(filter);
    Phase2Filter *phase2 = phase5_phase2(filter);
    if (!filter || !phase35 || !phase2)
        return;

    const bool cursor_needs_clicks =
        phase35->cursor_enabled.load(std::memory_order_acquire);
    const bool spotlight_needs_clicks =
        filter->spotlight_enabled.load(std::memory_order_acquire) &&
        static_cast<arzoom::SpotlightMode>(
            filter->spotlight_mode.load(std::memory_order_acquire)) ==
            arzoom::SpotlightMode::Click;

    /* This Phase 2 flag means "capture even if click rings are disabled".
     * Preserve Presentation Cursor ownership while adding Click Spotlight. */
    phase2->click_capture_for_cursor.store(
        cursor_needs_clicks || spotlight_needs_clicks,
        std::memory_order_release);
}

void phase5_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter || !filter->phase41)
        return;

    /* Click mode must request sampling before the inherited P2 tick executes. */
    phase5_prepare_click_sampling(filter);
    phase41_scene_context_tick(filter->phase41, seconds);

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    ArZoomFilter *phase1 = phase5_phase1(filter);
    if (!phase1)
        return;

    if (filter->runtime_reset_requested.exchange(
            false, std::memory_order_acq_rel)) {
        filter->center_valid = false;
        filter->click_anchor_valid = false;
        filter->last_click_generation = 0;
        filter->runtime_active.store(false, std::memory_order_release);
    }

    const bool globally_enabled =
        phase1->enabled.load(std::memory_order_acquire);
    const bool preview =
        filter->preview_requested.load(std::memory_order_acquire);
    const bool enabled =
        filter->spotlight_enabled.load(std::memory_order_acquire);

    if (!globally_enabled || (!enabled && !preview) || !filter->shader_ready) {
        filter->runtime_active.store(false, std::memory_order_release);
        return;
    }

    if (preview) {
        move_spotlight_center(filter, {0.5f, 0.45f}, dt, 0.10f);
        filter->runtime_active.store(true, std::memory_order_release);
        return;
    }

    const auto mode = static_cast<arzoom::SpotlightMode>(
        filter->spotlight_mode.load(std::memory_order_acquire));

    if (mode == arzoom::SpotlightMode::Click) {
        capture_latest_spotlight_click(filter);
        if (!filter->click_anchor_valid) {
            filter->runtime_active.store(false, std::memory_order_release);
            return;
        }
        move_spotlight_center(
            filter,
            content_to_live_output(phase1, filter->click_anchor_content),
            dt, 0.0f);
        filter->runtime_active.store(true, std::memory_order_release);
        return;
    }

    arzoom::Vec2 pointer_content;
    if (!mapped_pointer_content(phase1, pointer_content)) {
        /* Mapping failure is fail-safe: never guess. A previously proven
         * focus may remain held; first-use without mapping stays inactive. */
        filter->runtime_active.store(filter->center_valid,
                                     std::memory_order_release);
        return;
    }

    const arzoom::Vec2 pointer_output =
        content_to_live_output(phase1, pointer_content);

    if (mode == arzoom::SpotlightMode::Cursor) {
        move_spotlight_center(filter, pointer_output, dt, 0.055f);
        filter->runtime_active.store(true, std::memory_order_release);
        return;
    }

    /* Smart Focus shares the accepted camera context threshold instead of
     * inventing a second pointer planner. Local motion inside the useful
     * context leaves the focus still. Meaningful relocation, or an already
     * active camera semantic move, lets the visual focus travel. This stage is
     * read-only and cannot feed authority back into SceneViewportPlanner. */
    if (!filter->center_valid) {
        move_spotlight_center(filter, pointer_output, dt, 0.0f);
    } else {
        const arzoom::Vec2 current{
            filter->center_output_x, filter->center_output_y};
        const float distance_output = arzoom::length(
            arzoom::sub(pointer_output, current));
        const float wake = arzoom::scene_context_wake_half(
            phase1->safe_zone.load(std::memory_order_acquire),
            std::max(phase1->current_zoom, 1.0f));
        const bool meaningful_relocation =
            camera_is_semantically_moving(phase1) ||
            distance_output >= wake;
        if (meaningful_relocation)
            move_spotlight_center(filter, pointer_output, dt, 0.125f);
    }

    filter->runtime_active.store(filter->center_valid,
                                 std::memory_order_release);
}

bool phase5_preview_clicked(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter)
        return false;
    const bool current =
        filter->preview_requested.load(std::memory_order_acquire);
    filter->preview_requested.store(!current, std::memory_order_release);
    if (!current)
        filter->runtime_reset_requested.store(true, std::memory_order_release);
    return false;
}

void phase5_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter || !filter->phase41)
        return;

    phase41_update(filter->phase41, settings);

    filter->spotlight_enabled.store(
        obs_data_get_bool(settings, SETTING_SPOTLIGHT_ENABLED),
        std::memory_order_release);
    filter->spotlight_mode.store(
        static_cast<int>(parse_spotlight_mode(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_MODE))),
        std::memory_order_release);
    filter->spotlight_size.store(
        static_cast<int>(parse_spotlight_size(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_SIZE))),
        std::memory_order_release);
    filter->spotlight_dim.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SPOTLIGHT_DIM)) /
                       100.0f,
                   0.0f, 0.75f),
        std::memory_order_release);
    filter->spotlight_shape.store(
        static_cast<int>(parse_spotlight_shape(
            obs_data_get_string(settings, SETTING_SPOTLIGHT_SHAPE))),
        std::memory_order_release);
    filter->spotlight_feather_px.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SPOTLIGHT_FEATHER)),
                   24.0f, 220.0f),
        std::memory_order_release);

    filter->runtime_reset_requested.store(true, std::memory_order_release);
}

void phase5_defaults(obs_data_t *settings)
{
    phase352_defaults(settings);
    obs_data_set_default_bool(settings, SETTING_SPOTLIGHT_ENABLED, false);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_MODE,
                                SPOTLIGHT_MODE_SMART);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_SIZE,
                                SPOTLIGHT_SIZE_BALANCED);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_DIM, 38);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_SHAPE,
                                SPOTLIGHT_SHAPE_ELLIPSE);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_FEATHER, 92);
}

obs_properties_t *phase5_properties(void *data)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    obs_properties_t *props = phase41_properties(
        filter ? filter->phase41 : nullptr);

    obs_properties_t *spotlight = obs_properties_create();

    obs_property_t *info = obs_properties_add_text(
        spotlight, "spotlight_info",
        obs_module_text("ArZoom.Spotlight.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    obs_properties_add_bool(
        spotlight, SETTING_SPOTLIGHT_ENABLED,
        obs_module_text("ArZoom.Spotlight.Enabled"));

    obs_property_t *mode = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_MODE,
        obs_module_text("ArZoom.Spotlight.Mode"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Smart"),
        SPOTLIGHT_MODE_SMART);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Cursor"),
        SPOTLIGHT_MODE_CURSOR);
    obs_property_list_add_string(
        mode, obs_module_text("ArZoom.Spotlight.Mode.Click"),
        SPOTLIGHT_MODE_CLICK);

    obs_property_t *size = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_SIZE,
        obs_module_text("ArZoom.Spotlight.Size"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        size, obs_module_text("ArZoom.Spotlight.Size.Compact"),
        SPOTLIGHT_SIZE_COMPACT);
    obs_property_list_add_string(
        size, obs_module_text("ArZoom.Spotlight.Size.Balanced"),
        SPOTLIGHT_SIZE_BALANCED);
    obs_property_list_add_string(
        size, obs_module_text("ArZoom.Spotlight.Size.Wide"),
        SPOTLIGHT_SIZE_WIDE);

    obs_property_t *dim = obs_properties_add_int_slider(
        spotlight, SETTING_SPOTLIGHT_DIM,
        obs_module_text("ArZoom.Spotlight.Dim"), 15, 60, 1);
    obs_property_int_set_suffix(dim, " %");

    if (filter) {
        obs_properties_add_button2(
            spotlight, "spotlight_preview",
            obs_module_text("ArZoom.Spotlight.Preview"),
            phase5_preview_clicked, filter);
    }

    obs_properties_t *advanced = obs_properties_create();
    obs_property_t *shape = obs_properties_add_list(
        advanced, SETTING_SPOTLIGHT_SHAPE,
        obs_module_text("ArZoom.Spotlight.Shape"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.Ellipse"),
        SPOTLIGHT_SHAPE_ELLIPSE);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.RoundedRect"),
        SPOTLIGHT_SHAPE_ROUNDED_RECT);

    obs_property_t *feather = obs_properties_add_int_slider(
        advanced, SETTING_SPOTLIGHT_FEATHER,
        obs_module_text("ArZoom.Spotlight.Feather"), 30, 180, 2);
    obs_property_int_set_suffix(feather, " px");

    obs_properties_add_group(
        spotlight, "spotlight_advanced",
        obs_module_text("ArZoom.Advanced"), OBS_GROUP_NORMAL, advanced);

    obs_properties_add_group(
        props, "spotlight_group", obs_module_text("ArZoom.Spotlight.Group"),
        OBS_GROUP_NORMAL, spotlight);
    return props;
}

void phase5_set_disabled_uniform(Phase5Filter *filter)
{
    if (filter && filter->enabled_param)
        gs_effect_set_float(filter->enabled_param, 0.0f);
}

void phase5_set_spotlight_uniforms(Phase5Filter *filter,
                                   float width, float height)
{
    const auto size = static_cast<arzoom::SpotlightSize>(
        filter->spotlight_size.load(std::memory_order_acquire));
    const arzoom::SpotlightVec2 half = arzoom::spotlight_half_size_px(
        size, width, height);

    vec2 center;
    vec2_set(&center,
             filter->render_center_x.load(std::memory_order_acquire),
             filter->render_center_y.load(std::memory_order_acquire));
    gs_effect_set_vec2(filter->center_param, &center);

    vec2 half_size;
    vec2_set(&half_size, half.x, half.y);
    gs_effect_set_vec2(filter->half_size_param, &half_size);

    const float feather =
        filter->spotlight_feather_px.load(std::memory_order_acquire);
    const float dim = filter->spotlight_dim.load(std::memory_order_acquire);
    const auto shape = static_cast<arzoom::SpotlightShape>(
        filter->spotlight_shape.load(std::memory_order_acquire));
    const float corner = std::clamp(
        std::min(half.x, half.y) * 0.24f, 24.0f, 72.0f);

    gs_effect_set_float(filter->feather_param, feather);
    gs_effect_set_float(filter->dim_param, dim);
    gs_effect_set_float(
        filter->shape_param,
        shape == arzoom::SpotlightShape::RoundedRectangle ? 1.0f : 0.0f);
    gs_effect_set_float(filter->corner_param, corner);
    gs_effect_set_float(filter->enabled_param, 1.0f);
}

void phase5_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter || !filter->phase41)
        return;

    const bool spotlight_active =
        filter->runtime_active.load(std::memory_order_acquire) &&
        filter->shader_ready;

    if (!spotlight_active) {
        phase5_set_disabled_uniform(filter);
        phase41_cursor_scaled_render(filter->phase41, effect);
        return;
    }

    Phase35Filter *phase35 = phase5_phase35(filter);
    Phase2Filter *phase2 = phase5_phase2(filter);
    Phase351Filter *phase351 = phase5_phase351(filter);
    ArZoomFilter *phase1 = phase5_phase1(filter);
    if (!phase35 || !phase2 || !phase351 || !phase1 ||
        !phase1->effect_ready || !phase1->effect ||
        !phase1->enabled.load(std::memory_order_acquire)) {
        phase5_set_disabled_uniform(filter);
        phase41_cursor_scaled_render(filter->phase41, effect);
        return;
    }

    prime_cursor_sampler_safety(filter->phase41->phase4);

    const float base_cursor_size =
        phase351->base_cursor_size_px.load(std::memory_order_acquire);
    phase35->cursor_size_px.store(
        arzoom::presentation_cursor_scaled_height(
            base_cursor_size, std::max(phase1->current_zoom, 1.0f)),
        std::memory_order_release);

    bool cursor_active =
        phase35->cursor_enabled.load(std::memory_order_acquire) &&
        phase35->cursor_position_valid.load(std::memory_order_acquire) &&
        phase35->cursor_shader_ready;

    std::unique_lock<std::mutex> resource_lock(
        phase35->asset_mutex, std::defer_lock);
    if (cursor_active) {
        resource_lock.lock();
        if (!phase35->atlas_texture || phase35->frame_count == 0)
            cursor_active = false;
    }

    if (!obs_source_process_filter_begin(
            phase1->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        phase5_set_disabled_uniform(filter);
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    gs_effect_set_float(phase1->zoom_param,
                        std::max(phase1->current_zoom, 1.0f));
    vec2 camera_center;
    vec2_set(&camera_center, phase1->current_center.x,
             phase1->current_center.y);
    gs_effect_set_vec2(phase1->center_param, &camera_center);

    obs_source_t *target = obs_filter_get_target(phase1->context);
    const float width = static_cast<float>(
        target ? std::max(obs_source_get_width(target), 1u) : 1u);
    const float height = static_cast<float>(
        target ? std::max(obs_source_get_height(target), 1u) : 1u);

    if (phase2->viewport_size_param) {
        vec2 viewport;
        vec2_set(&viewport, width, height);
        gs_effect_set_vec2(phase2->viewport_size_param, &viewport);
    }

    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i)
        set_click_uniform(phase2->click_params[i], phase2->clicks.slot(i));

    phase5_set_spotlight_uniforms(filter, width, height);

    if (cursor_active) {
        gs_effect_set_texture(phase35->cursor_atlas_param,
                              phase35->atlas_texture);

        vec2 content;
        vec2_set(&content,
                 phase35->cursor_content_x.load(std::memory_order_acquire),
                 phase35->cursor_content_y.load(std::memory_order_acquire));
        gs_effect_set_vec2(phase35->cursor_content_param, &content);

        vec2 asset_size;
        vec2_set(&asset_size,
                 static_cast<float>(phase35->frame_width),
                 static_cast<float>(phase35->frame_height));
        gs_effect_set_vec2(phase35->cursor_asset_size_param, &asset_size);

        vec2 hotspot;
        vec2_set(&hotspot,
                 phase35->hotspot_x.load(std::memory_order_acquire),
                 phase35->hotspot_y.load(std::memory_order_acquire));
        gs_effect_set_vec2(phase35->cursor_hotspot_param, &hotspot);

        vec2 atlas_grid;
        vec2_set(&atlas_grid,
                 static_cast<float>(phase35->atlas_columns),
                 static_cast<float>(phase35->atlas_rows));
        gs_effect_set_vec2(phase35->cursor_atlas_grid_param, &atlas_grid);

        const int safe_frame = std::clamp(
            phase35->current_frame.load(std::memory_order_acquire),
            0, static_cast<int>(phase35->frame_count - 1));
        gs_effect_set_float(phase35->cursor_frame_param,
                            static_cast<float>(safe_frame));
        gs_effect_set_float(
            phase35->cursor_size_param,
            phase35->cursor_size_px.load(std::memory_order_acquire));
        gs_effect_set_float(phase35->cursor_visible_param, 1.0f);
    } else {
        set_cursor_hidden(phase35);
    }

    obs_source_process_filter_end(
        phase1->context, phase1->effect, 0, 0);
}

void phase5_deactivate(void *data)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter)
        return;
    filter->preview_requested.store(false, std::memory_order_release);
    filter->runtime_reset_requested.store(true, std::memory_order_release);
    filter->runtime_active.store(false, std::memory_order_release);
    phase41_deactivate(filter->phase41);
}

void phase5_destroy(void *data)
{
    auto *filter = static_cast<Phase5Filter *>(data);
    if (!filter)
        return;
    phase41_destroy(filter->phase41);
    delete filter;
}

void *phase5_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase41 = static_cast<Phase41Filter *>(
        phase41_create(settings, context));
    if (!phase41)
        return nullptr;

    auto *filter = new (std::nothrow) Phase5Filter();
    if (!filter) {
        phase41_destroy(phase41);
        return nullptr;
    }
    filter->phase41 = phase41;

    ArZoomFilter *phase1 = phase5_phase1(filter);
    if (phase1 && phase1->effect) {
        filter->enabled_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_enabled");
        filter->center_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_center");
        filter->half_size_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_half_size_px");
        filter->feather_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_feather_px");
        filter->dim_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_dim_strength");
        filter->shape_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_shape");
        filter->corner_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_corner_radius_px");
        filter->shader_ready =
            filter->enabled_param && filter->center_param &&
            filter->half_size_param && filter->feather_param &&
            filter->dim_param && filter->shape_param && filter->corner_param;
    }

    if (!filter->shader_ready) {
        blog(LOG_WARNING,
             "[ArZoom] P5 Spotlight shader parameters unavailable. "
             "The accepted v0.6.0 camera/click/cursor path remains active.");
    } else {
        phase5_set_disabled_uniform(filter);
        blog(LOG_INFO,
             "[ArZoom] P5 one-pass analytic Spotlight runtime ready");
    }

    phase5_update(filter, settings);
    return filter;
}

struct Phase5SourceInfoOverride {
    Phase5SourceInfoOverride()
    {
        arzoom_filter_info.create = phase5_create;
        arzoom_filter_info.destroy = phase5_destroy;
        arzoom_filter_info.video_tick = phase5_tick;
        arzoom_filter_info.video_render = phase5_render;
        arzoom_filter_info.update = phase5_update;
        arzoom_filter_info.get_properties = phase5_properties;
        arzoom_filter_info.get_defaults = phase5_defaults;
        arzoom_filter_info.deactivate = phase5_deactivate;
    }
};

Phase5SourceInfoOverride phase5_source_info_override;

} // namespace
