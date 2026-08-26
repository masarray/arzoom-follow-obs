#include "arzoom-filter-v15.cpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#define SETTING_SPOTLIGHT_AREA_SCALE "spotlight_area_scale"

#define SPOTLIGHT_SHAPE_CIRCLE "circle"

/*
 * P5.1 Spotlight direct-OBS hardening
 * ===================================
 *
 * Trial feedback exposed five lifecycle/UX faults in the first P5 wrapper:
 *   1) the first-click black-frame regression reappeared while Spotlight owned
 *      the shared presentation pass;
 *   2) master Enable put Spotlight on-air immediately;
 *   3) a true circle was missing;
 *   4) area size was not obvious/continuous;
 *   5) startup/mode changes could retain stale focus state until Preview was
 *      toggled.
 *
 * Keep v15 as the trial snapshot and layer this deterministic hotfix above it.
 * P5.1 owns presenter activation + mode lifecycle only. Camera/mapping remain
 * read-only dependencies and Spotlight still cannot write camera intent.
 */
namespace {

enum class Phase51Shape : int {
    Circle = 0,
    Ellipse,
    RoundedRectangle,
};

struct Phase51Filter {
    Phase5Filter *phase5 = nullptr;

    /* Explicit presenter intent. Master `spotlight_enabled` is configuration,
     * never an on-air command by itself. */
    std::atomic<bool> latched_active{false};
    std::atomic<bool> hold_active{false};
    std::atomic<bool> gui_peek_requested{false};

    std::atomic<int> shape{static_cast<int>(Phase51Shape::Circle)};
    std::atomic<float> area_scale_percent{100.0f};

    /* GUI/settings callbacks publish transitions; video tick owns geometry. */
    std::atomic<uint32_t> transition_generation{1};
    uint32_t applied_transition_generation = 0;
    float gui_peek_remaining = 0.0f;
    bool last_runtime_requested = false;

    bool center_valid = false;
    arzoom::Vec2 center_output{0.5f, 0.45f};
    bool click_anchor_valid = false;
    arzoom::Vec2 click_anchor_content{0.5f, 0.5f};
    uint32_t last_click_generation = 0;

    gs_eparam_t *area_scale_param = nullptr;
    gs_eparam_t *circle_param = nullptr;
    bool extension_shader_ready = false;
};

std::mutex phase51_registry_mutex;
std::vector<Phase51Filter *> phase51_registry;

Phase35Filter *phase51_phase35(Phase51Filter *filter)
{
    return filter && filter->phase5 ? phase5_phase35(filter->phase5) : nullptr;
}

Phase2Filter *phase51_phase2(Phase51Filter *filter)
{
    return filter && filter->phase5 ? phase5_phase2(filter->phase5) : nullptr;
}

ArZoomFilter *phase51_phase1(Phase51Filter *filter)
{
    return filter && filter->phase5 ? phase5_phase1(filter->phase5) : nullptr;
}

void register_phase51_instance(Phase51Filter *filter)
{
    std::lock_guard<std::mutex> lock(phase51_registry_mutex);
    phase51_registry.push_back(filter);
}

void unregister_phase51_instance(Phase51Filter *filter)
{
    std::lock_guard<std::mutex> lock(phase51_registry_mutex);
    phase51_registry.erase(
        std::remove(phase51_registry.begin(), phase51_registry.end(), filter),
        phase51_registry.end());
}

bool phase51_available(Phase51Filter *filter)
{
    ArZoomFilter *phase1 = phase51_phase1(filter);
    return filter && filter->phase5 && phase1 &&
           phase1->enabled.load(std::memory_order_acquire) &&
           filter->phase5->spotlight_enabled.load(std::memory_order_acquire);
}

template<typename Fn>
void for_phase51_targets(Fn &&fn)
{
    std::lock_guard<std::mutex> lock(phase51_registry_mutex);

    bool has_showing = false;
    for (Phase51Filter *filter : phase51_registry) {
        ArZoomFilter *phase1 = phase51_phase1(filter);
        if (!phase51_available(filter) || !phase1)
            continue;
        if (obs_source_showing(phase1->context)) {
            has_showing = true;
            break;
        }
    }

    for (Phase51Filter *filter : phase51_registry) {
        ArZoomFilter *phase1 = phase51_phase1(filter);
        if (!phase51_available(filter) || !phase1)
            continue;
        if (has_showing && !obs_source_showing(phase1->context))
            continue;
        fn(filter);
    }
}

template<typename Fn>
void for_all_phase51_instances(Fn &&fn)
{
    std::lock_guard<std::mutex> lock(phase51_registry_mutex);
    for (Phase51Filter *filter : phase51_registry) {
        if (filter)
            fn(filter);
    }
}

Phase51Shape parse_phase51_shape(const char *value)
{
    if (value && std::strcmp(value, SPOTLIGHT_SHAPE_ROUNDED_RECT) == 0)
        return Phase51Shape::RoundedRectangle;
    if (value && std::strcmp(value, SPOTLIGHT_SHAPE_ELLIPSE) == 0)
        return Phase51Shape::Ellipse;
    return Phase51Shape::Circle;
}

uint32_t newest_phase51_click_generation(Phase2Filter *phase2)
{
    uint32_t newest = 0;
    if (!phase2)
        return newest;
    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i) {
        const arzoom::ClickEvent &event = phase2->clicks.slot(i);
        if (event.active())
            newest = std::max(newest, event.generation);
    }
    return newest;
}

void reset_phase51_visual_state(Phase51Filter *filter)
{
    if (!filter)
        return;
    filter->center_valid = false;
    filter->center_output = {0.5f, 0.45f};
    filter->click_anchor_valid = false;
    filter->click_anchor_content = {0.5f, 0.5f};
    filter->last_click_generation =
        newest_phase51_click_generation(phase51_phase2(filter));
    if (filter->phase5) {
        filter->phase5->runtime_active.store(false,
                                             std::memory_order_release);
    }
}

void publish_phase51_center(Phase51Filter *filter)
{
    if (!filter || !filter->phase5 || !filter->center_valid)
        return;
    filter->phase5->render_center_x.store(filter->center_output.x,
                                          std::memory_order_release);
    filter->phase5->render_center_y.store(filter->center_output.y,
                                          std::memory_order_release);
}

void move_phase51_center(Phase51Filter *filter, arzoom::Vec2 target,
                         float dt, float time_constant)
{
    if (!filter)
        return;

    target.x = std::clamp(target.x, 0.0f, 1.0f);
    target.y = std::clamp(target.y, 0.0f, 1.0f);

    if (!filter->center_valid || time_constant <= 0.001f) {
        filter->center_output = target;
        filter->center_valid = true;
    } else {
        const float alpha = spotlight_visual_alpha(dt, time_constant);
        filter->center_output.x +=
            (target.x - filter->center_output.x) * alpha;
        filter->center_output.y +=
            (target.y - filter->center_output.y) * alpha;
    }
    publish_phase51_center(filter);
}

bool capture_phase51_click(Phase51Filter *filter)
{
    Phase2Filter *phase2 = phase51_phase2(filter);
    if (!filter || !phase2)
        return false;

    const arzoom::ClickEvent *newest = nullptr;
    for (size_t i = 0; i < arzoom::ClickVisualState::kSlotCount; ++i) {
        const arzoom::ClickEvent &event = phase2->clicks.slot(i);
        if (!event.active() || event.generation <= filter->last_click_generation)
            continue;
        if (!newest || event.generation > newest->generation)
            newest = &event;
    }
    if (!newest)
        return false;

    filter->last_click_generation = newest->generation;
    filter->click_anchor_content = newest->content_position;
    filter->click_anchor_valid = true;
    return true;
}

bool phase51_pointer_output(Phase51Filter *filter, arzoom::Vec2 &output)
{
    ArZoomFilter *phase1 = phase51_phase1(filter);
    arzoom::Vec2 content;
    if (!phase1 || !mapped_pointer_content(phase1, content))
        return false;
    output = content_to_live_output(phase1, content);
    return std::isfinite(output.x) && std::isfinite(output.y);
}

void phase51_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter || !filter->phase5 || !filter->phase5->phase41)
        return;

    /* Preserve the accepted P4.1 camera/click/cursor tick path. Click Spotlight
     * may request the same bounded sampler before inherited capture executes. */
    phase5_prepare_click_sampling(filter->phase5);
    phase41_scene_context_tick(filter->phase5->phase41, seconds);

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    ArZoomFilter *phase1 = phase51_phase1(filter);
    if (!phase1)
        return;

    const uint32_t generation =
        filter->transition_generation.load(std::memory_order_acquire);
    if (generation != filter->applied_transition_generation) {
        reset_phase51_visual_state(filter);
        filter->gui_peek_remaining = 0.0f;
        filter->applied_transition_generation = generation;
    }

    if (filter->gui_peek_requested.exchange(
            false, std::memory_order_acq_rel)) {
        /* OBS property buttons provide click callbacks, not press/release
         * callbacks. Give the GUI a short deterministic peek; the Hold
         * Spotlight hotkey below provides true press-and-hold semantics. */
        filter->gui_peek_remaining = 1.25f;
    }
    if (filter->gui_peek_remaining > 0.0f)
        filter->gui_peek_remaining =
            std::max(0.0f, filter->gui_peek_remaining - dt);

    const bool master_enabled =
        filter->phase5->spotlight_enabled.load(std::memory_order_acquire);
    const bool latched =
        filter->latched_active.load(std::memory_order_acquire);
    const bool held =
        filter->hold_active.load(std::memory_order_acquire);
    const bool peek = filter->gui_peek_remaining > 0.0f;
    const bool runtime_requested = arzoom::spotlight_runtime_requested(
        master_enabled, latched, held, peek);

    if (!runtime_requested || !filter->phase5->shader_ready ||
        !filter->extension_shader_ready ||
        !phase1->enabled.load(std::memory_order_acquire)) {
        filter->phase5->runtime_active.store(false,
                                             std::memory_order_release);
        filter->last_runtime_requested = false;
        return;
    }

    if (!filter->last_runtime_requested) {
        /* Activation always starts from current context; never resurrect stale
         * startup/mode geometry from a previous hidden Spotlight session. */
        reset_phase51_visual_state(filter);
    }
    filter->last_runtime_requested = true;

    arzoom::Vec2 pointer_output;
    const bool pointer_valid = phase51_pointer_output(filter, pointer_output);

    if (peek) {
        move_phase51_center(filter,
                            pointer_valid ? pointer_output
                                          : arzoom::Vec2{0.5f, 0.45f},
                            dt, 0.060f);
        filter->phase5->runtime_active.store(true,
                                             std::memory_order_release);
        return;
    }

    const auto mode = static_cast<arzoom::SpotlightMode>(
        filter->phase5->spotlight_mode.load(std::memory_order_acquire));

    if (mode == arzoom::SpotlightMode::Click) {
        capture_phase51_click(filter);
        if (!filter->click_anchor_valid) {
            filter->phase5->runtime_active.store(false,
                                                 std::memory_order_release);
            return;
        }
        move_phase51_center(
            filter,
            content_to_live_output(phase1, filter->click_anchor_content),
            dt, 0.0f);
        filter->phase5->runtime_active.store(true,
                                             std::memory_order_release);
        return;
    }

    if (!pointer_valid) {
        filter->phase5->runtime_active.store(filter->center_valid,
                                             std::memory_order_release);
        return;
    }

    if (mode == arzoom::SpotlightMode::Cursor) {
        move_phase51_center(filter, pointer_output, dt, 0.055f);
        filter->phase5->runtime_active.store(true,
                                             std::memory_order_release);
        return;
    }

    /* Smart Focus stays quiet during local pointing. Meaningful relocation uses
     * the same accepted context wake threshold as the camera, but remains a
     * read-only presentation consumer. */
    if (!filter->center_valid) {
        move_phase51_center(filter, pointer_output, dt, 0.0f);
    } else {
        const float distance_output = arzoom::length(
            arzoom::sub(pointer_output, filter->center_output));
        const float wake = arzoom::scene_context_wake_half(
            phase1->safe_zone.load(std::memory_order_acquire),
            std::max(phase1->current_zoom, 1.0f));
        if (camera_is_semantically_moving(phase1) || distance_output >= wake)
            move_phase51_center(filter, pointer_output, dt, 0.125f);
    }

    filter->phase5->runtime_active.store(filter->center_valid,
                                         std::memory_order_release);
}

bool phase51_toggle_button(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!phase51_available(filter))
        return false;
    const bool current =
        filter->latched_active.load(std::memory_order_acquire);
    filter->latched_active.store(!current, std::memory_order_release);
    filter->transition_generation.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

bool phase51_peek_button(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!phase51_available(filter))
        return false;
    filter->gui_peek_requested.store(true, std::memory_order_release);
    return false;
}

void phase51_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter || !filter->phase5 || !filter->phase5->phase41)
        return;

    phase41_update(filter->phase5->phase41, settings);

    const bool previous_master =
        filter->phase5->spotlight_enabled.load(std::memory_order_acquire);
    const bool next_master =
        obs_data_get_bool(settings, SETTING_SPOTLIGHT_ENABLED);
    filter->phase5->spotlight_enabled.store(next_master,
                                             std::memory_order_release);

    const int previous_mode =
        filter->phase5->spotlight_mode.load(std::memory_order_acquire);
    const int next_mode = static_cast<int>(parse_spotlight_mode(
        obs_data_get_string(settings, SETTING_SPOTLIGHT_MODE)));
    filter->phase5->spotlight_mode.store(next_mode,
                                         std::memory_order_release);

    /* P5.1 uses one obvious continuous size control. Keep the old internal
     * preset fixed at Balanced so stale trial settings cannot surprise users. */
    filter->phase5->spotlight_size.store(
        static_cast<int>(arzoom::SpotlightSize::Balanced),
        std::memory_order_release);

    filter->phase5->spotlight_dim.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SPOTLIGHT_DIM)) /
                       100.0f,
                   0.0f, 0.75f),
        std::memory_order_release);
    filter->phase5->spotlight_feather_px.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SPOTLIGHT_FEATHER)),
                   24.0f, 220.0f),
        std::memory_order_release);

    const Phase51Shape next_shape = parse_phase51_shape(
        obs_data_get_string(settings, SETTING_SPOTLIGHT_SHAPE));
    filter->shape.store(static_cast<int>(next_shape),
                        std::memory_order_release);
    filter->area_scale_percent.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SPOTLIGHT_AREA_SCALE)),
                   50.0f, 200.0f),
        std::memory_order_release);

    /* v15 shader distinguishes rounded rectangle from ellipse. Circle is the
     * ellipse SDF with equal radii enforced by the P5.1 shader extension. */
    filter->phase5->spotlight_shape.store(
        static_cast<int>(
            next_shape == Phase51Shape::RoundedRectangle
                ? arzoom::SpotlightShape::RoundedRectangle
                : arzoom::SpotlightShape::Ellipse),
        std::memory_order_release);

    if (previous_mode != next_mode || previous_master != next_master)
        filter->transition_generation.fetch_add(1, std::memory_order_acq_rel);

    if (!next_master) {
        filter->latched_active.store(false, std::memory_order_release);
        filter->hold_active.store(false, std::memory_order_release);
        filter->phase5->runtime_active.store(false,
                                             std::memory_order_release);
    }
}

void phase51_defaults(obs_data_t *settings)
{
    phase352_defaults(settings);
    obs_data_set_default_bool(settings, SETTING_SPOTLIGHT_ENABLED, false);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_MODE,
                                SPOTLIGHT_MODE_SMART);
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_SHAPE,
                                SPOTLIGHT_SHAPE_CIRCLE);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_AREA_SCALE, 100);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_DIM, 38);
    obs_data_set_default_int(settings, SETTING_SPOTLIGHT_FEATHER, 92);
    /* Retain the old key only for settings-file compatibility. */
    obs_data_set_default_string(settings, SETTING_SPOTLIGHT_SIZE,
                                SPOTLIGHT_SIZE_BALANCED);
}

obs_properties_t *phase51_properties(void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    obs_properties_t *props = phase41_properties(
        filter && filter->phase5 ? filter->phase5->phase41 : nullptr);

    obs_properties_t *spotlight = obs_properties_create();

    obs_property_t *info = obs_properties_add_text(
        spotlight, "spotlight_info",
        obs_module_text("ArZoom.Spotlight.Info.P51"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    obs_properties_add_bool(
        spotlight, SETTING_SPOTLIGHT_ENABLED,
        obs_module_text("ArZoom.Spotlight.Enabled.P51"));

    if (filter) {
        const bool on = filter->latched_active.load(std::memory_order_acquire) ||
                        filter->hold_active.load(std::memory_order_acquire);
        obs_property_t *status = obs_properties_add_text(
            spotlight, "spotlight_runtime_status",
            obs_module_text(on ? "ArZoom.Spotlight.Status.On"
                               : "ArZoom.Spotlight.Status.Off"),
            OBS_TEXT_INFO);
        obs_property_text_set_info_type(status, OBS_TEXT_INFO_NORMAL);
        obs_property_text_set_info_word_wrap(status, true);
    }

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

    obs_property_t *shape = obs_properties_add_list(
        spotlight, SETTING_SPOTLIGHT_SHAPE,
        obs_module_text("ArZoom.Spotlight.Shape"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.Circle"),
        SPOTLIGHT_SHAPE_CIRCLE);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.Ellipse"),
        SPOTLIGHT_SHAPE_ELLIPSE);
    obs_property_list_add_string(
        shape, obs_module_text("ArZoom.Spotlight.Shape.RoundedRect"),
        SPOTLIGHT_SHAPE_ROUNDED_RECT);

    obs_property_t *area = obs_properties_add_int_slider(
        spotlight, SETTING_SPOTLIGHT_AREA_SCALE,
        obs_module_text("ArZoom.Spotlight.AreaSize"), 50, 200, 5);
    obs_property_int_set_suffix(area, " %");

    obs_property_t *dim = obs_properties_add_int_slider(
        spotlight, SETTING_SPOTLIGHT_DIM,
        obs_module_text("ArZoom.Spotlight.Dim"), 15, 60, 1);
    obs_property_int_set_suffix(dim, " %");

    if (filter) {
        obs_properties_add_button2(
            spotlight, "spotlight_toggle_now",
            obs_module_text("ArZoom.Spotlight.ToggleButton"),
            phase51_toggle_button, filter);
        obs_properties_add_button2(
            spotlight, "spotlight_peek_now",
            obs_module_text("ArZoom.Spotlight.PeekButton"),
            phase51_peek_button, filter);
    }

    obs_properties_t *advanced = obs_properties_create();
    obs_property_t *feather = obs_properties_add_int_slider(
        advanced, SETTING_SPOTLIGHT_FEATHER,
        obs_module_text("ArZoom.Spotlight.Feather"), 30, 180, 2);
    obs_property_int_set_suffix(feather, " px");
    obs_property_t *hotkey_info = obs_properties_add_text(
        advanced, "spotlight_hotkey_info",
        obs_module_text("ArZoom.Spotlight.HotkeyInfo"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(hotkey_info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(hotkey_info, true);
    obs_properties_add_button(
        advanced, "spotlight_open_hotkeys",
        obs_module_text("ArZoom.OpenHotkeys"), open_hotkeys_clicked);

    obs_properties_add_group(
        spotlight, "spotlight_advanced",
        obs_module_text("ArZoom.Advanced"), OBS_GROUP_NORMAL, advanced);

    obs_properties_add_group(
        props, "spotlight_group", obs_module_text("ArZoom.Spotlight.Group"),
        OBS_GROUP_NORMAL, spotlight);
    return props;
}

void phase51_set_extension_uniforms(Phase51Filter *filter)
{
    if (!filter || !filter->extension_shader_ready)
        return;
    const float scale = std::clamp(
        filter->area_scale_percent.load(std::memory_order_acquire),
        50.0f, 200.0f) / 100.0f;
    const auto shape = static_cast<Phase51Shape>(
        filter->shape.load(std::memory_order_acquire));
    gs_effect_set_float(filter->area_scale_param, scale);
    gs_effect_set_float(filter->circle_param,
                        shape == Phase51Shape::Circle ? 1.0f : 0.0f);
}

void phase51_force_first_click_sampler_safety(Phase51Filter *filter)
{
    if (!filter || !filter->phase5 || !filter->phase5->phase41 ||
        !filter->phase5->phase41->phase4)
        return;

    Phase35Filter *cursor = phase51_phase35(filter);
    if (!cursor)
        return;

    bool atlas_ready = false;
    {
        std::lock_guard<std::mutex> lock(cursor->asset_mutex);
        atlas_ready = cursor->atlas_texture != nullptr &&
                      cursor->frame_width > 0 && cursor->frame_height > 0 &&
                      cursor->frame_count > 0;
    }
    const bool cursor_ready =
        cursor->cursor_enabled.load(std::memory_order_acquire) &&
        cursor->cursor_position_valid.load(std::memory_order_acquire) &&
        cursor->cursor_shader_ready && atlas_ready;

    const bool spotlight_active =
        filter->phase5->runtime_active.load(std::memory_order_acquire);
    if (!arzoom::spotlight_shared_pass_needs_cursor_fallback(
            spotlight_active, cursor_ready)) {
        return;
    }

    /* v0.5.0 fixed the exact black-frame class by binding a permanent 1x1
     * transparent cursor sampler. P5's always-active mask pass exposed that
     * contract again. Force a fresh bind on every non-ready Spotlight frame so
     * no stale/speculative sampler state can survive a Display Capture click. */
    filter->phase5->phase41->phase4->cursor_fallback_bound = false;
    prime_cursor_sampler_safety(filter->phase5->phase41->phase4);
}

void phase51_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter || !filter->phase5)
        return;

    phase51_set_extension_uniforms(filter);
    phase51_force_first_click_sampler_safety(filter);
    phase5_render(filter->phase5, effect);
}

void phase51_deactivate(void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter)
        return;
    filter->latched_active.store(false, std::memory_order_release);
    filter->hold_active.store(false, std::memory_order_release);
    filter->gui_peek_requested.store(false, std::memory_order_release);
    filter->transition_generation.fetch_add(1, std::memory_order_acq_rel);
    filter->gui_peek_remaining = 0.0f;
    filter->last_runtime_requested = false;
    phase5_deactivate(filter->phase5);
}

void phase51_destroy(void *data)
{
    auto *filter = static_cast<Phase51Filter *>(data);
    if (!filter)
        return;
    unregister_phase51_instance(filter);
    phase5_destroy(filter->phase5);
    delete filter;
}

void *phase51_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase5 = static_cast<Phase5Filter *>(
        phase5_create(settings, context));
    if (!phase5)
        return nullptr;

    auto *filter = new (std::nothrow) Phase51Filter();
    if (!filter) {
        phase5_destroy(phase5);
        return nullptr;
    }
    filter->phase5 = phase5;

    ArZoomFilter *phase1 = phase51_phase1(filter);
    if (phase1 && phase1->effect) {
        filter->area_scale_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_area_scale");
        filter->circle_param = gs_effect_get_param_by_name(
            phase1->effect, "spotlight_circle");
        filter->extension_shader_ready =
            filter->area_scale_param && filter->circle_param;
    }

    if (!filter->extension_shader_ready) {
        blog(LOG_WARNING,
             "[ArZoom] P5.1 Spotlight extension shader parameters unavailable; "
             "Spotlight remains fail-safe off.");
    }

    register_phase51_instance(filter);
    phase51_update(filter, settings);
    reset_phase51_visual_state(filter);
    filter->applied_transition_generation =
        filter->transition_generation.load(std::memory_order_acquire);

    blog(LOG_INFO,
         "[ArZoom] P5.1 Spotlight lifecycle/render-safety hotfix ready");
    return filter;
}

struct Phase51SourceInfoOverride {
    Phase51SourceInfoOverride()
    {
        arzoom_filter_info.create = phase51_create;
        arzoom_filter_info.destroy = phase51_destroy;
        arzoom_filter_info.video_tick = phase51_tick;
        arzoom_filter_info.video_render = phase51_render;
        arzoom_filter_info.update = phase51_update;
        arzoom_filter_info.get_properties = phase51_properties;
        arzoom_filter_info.get_defaults = phase51_defaults;
        arzoom_filter_info.deactivate = phase51_deactivate;
    }
};

Phase51SourceInfoOverride phase51_source_info_override;

/* ------------------------------------------------------------------------- */
/* Spotlight presenter hotkeys                                               */
/* ------------------------------------------------------------------------- */

enum class Phase51Hotkey : size_t {
    Toggle = 0,
    Hold,
    Count,
};

constexpr size_t kPhase51HotkeyCount =
    static_cast<size_t>(Phase51Hotkey::Count);

struct Phase51HotkeyDefinition {
    const char *config_name;
    const char *locale_key;
};

constexpr std::array<Phase51HotkeyDefinition, kPhase51HotkeyCount>
    phase51_hotkey_definitions{{
        {"arzoom.toggle_spotlight", "ArZoom.Hotkey.ToggleSpotlight"},
        {"arzoom.hold_spotlight", "ArZoom.Hotkey.HoldSpotlight"},
    }};

std::array<obs_hotkey_id, kPhase51HotkeyCount> phase51_hotkeys{{
    OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID,
}};
std::array<std::atomic<bool>, kPhase51HotkeyCount> phase51_hotkey_down{};
bool phase51_frontend_event_registered = false;

bool phase51_pressed_once(Phase51Hotkey hotkey, bool pressed)
{
    const size_t index = static_cast<size_t>(hotkey);
    if (!pressed) {
        phase51_hotkey_down[index].store(false, std::memory_order_release);
        return false;
    }
    return !phase51_hotkey_down[index].exchange(
        true, std::memory_order_acq_rel);
}

void phase51_toggle_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!phase51_pressed_once(Phase51Hotkey::Toggle, pressed))
        return;
    for_phase51_targets([](Phase51Filter *filter) {
        const bool current =
            filter->latched_active.load(std::memory_order_acquire);
        filter->latched_active.store(!current, std::memory_order_release);
        filter->transition_generation.fetch_add(1,
                                                std::memory_order_acq_rel);
    });
}

void phase51_hold_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    const size_t index = static_cast<size_t>(Phase51Hotkey::Hold);
    if (pressed) {
        if (phase51_hotkey_down[index].exchange(
                true, std::memory_order_acq_rel))
            return;
        for_phase51_targets([](Phase51Filter *filter) {
            filter->hold_active.store(true, std::memory_order_release);
            filter->transition_generation.fetch_add(1,
                                                    std::memory_order_acq_rel);
        });
        return;
    }

    phase51_hotkey_down[index].store(false, std::memory_order_release);
    /* Global release avoids a hidden/changed scene retaining a held mask. */
    for_all_phase51_instances([](Phase51Filter *filter) {
        filter->hold_active.store(false, std::memory_order_release);
        filter->transition_generation.fetch_add(1,
                                                std::memory_order_acq_rel);
    });
}

bool save_phase51_hotkeys_to_profile()
{
    config_t *config = obs_frontend_get_profile_config();
    if (!config)
        return false;

    for (size_t i = 0; i < kPhase51HotkeyCount; ++i) {
        if (phase51_hotkeys[i] == OBS_INVALID_HOTKEY_ID)
            continue;
        obs_data_array_t *bindings = obs_hotkey_save(phase51_hotkeys[i]);
        obs_data_t *wrapper = obs_data_create();
        if (!wrapper) {
            if (bindings)
                obs_data_array_release(bindings);
            continue;
        }
        if (bindings)
            obs_data_set_array(wrapper, HOTKEY_CONFIG_BINDINGS, bindings);
        const char *json = obs_data_get_json(wrapper);
        config_set_string(config, HOTKEY_CONFIG_SECTION,
                          phase51_hotkey_definitions[i].config_name,
                          json ? json : "");
        if (bindings)
            obs_data_array_release(bindings);
        obs_data_release(wrapper);
    }
    return config_save_safe(config, "tmp", nullptr) == CONFIG_SUCCESS;
}

bool load_phase51_hotkeys_from_profile()
{
    config_t *config = obs_frontend_get_profile_config();
    if (!config)
        return false;

    for (size_t i = 0; i < kPhase51HotkeyCount; ++i) {
        const obs_hotkey_id id = phase51_hotkeys[i];
        if (id == OBS_INVALID_HOTKEY_ID)
            continue;
        obs_hotkey_load_bindings(id, nullptr, 0);
        const char *json = config_get_string(
            config, HOTKEY_CONFIG_SECTION,
            phase51_hotkey_definitions[i].config_name);
        if (!json || !*json)
            continue;
        obs_data_t *wrapper = obs_data_create_from_json(json);
        if (!wrapper)
            continue;
        obs_data_array_t *bindings =
            obs_data_get_array(wrapper, HOTKEY_CONFIG_BINDINGS);
        if (bindings)
            obs_hotkey_load(id, bindings);
        if (bindings)
            obs_data_array_release(bindings);
        obs_data_release(wrapper);
    }
    return true;
}

void phase51_frontend_event(enum obs_frontend_event event, void *)
{
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
    case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
        load_phase51_hotkeys_from_profile();
        refresh_filter_properties();
        break;
    case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
    case OBS_FRONTEND_EVENT_EXIT:
        save_phase51_hotkeys_to_profile();
        break;
    default:
        break;
    }
}

} // namespace

bool arzoom_register_spotlight_hotkeys()
{
    static const std::array<obs_hotkey_func, kPhase51HotkeyCount> callbacks{{
        phase51_toggle_hotkey,
        phase51_hold_hotkey,
    }};

    bool all_registered = true;
    for (size_t i = 0; i < kPhase51HotkeyCount; ++i) {
        if (phase51_hotkeys[i] != OBS_INVALID_HOTKEY_ID)
            continue;
        phase51_hotkeys[i] = obs_hotkey_register_frontend(
            phase51_hotkey_definitions[i].config_name,
            obs_module_text(phase51_hotkey_definitions[i].locale_key),
            callbacks[i], nullptr);
        if (phase51_hotkeys[i] == OBS_INVALID_HOTKEY_ID) {
            all_registered = false;
            blog(LOG_ERROR,
                 "[ArZoom] Failed to register Spotlight hotkey: %s",
                 phase51_hotkey_definitions[i].config_name);
        }
    }

    if (!phase51_frontend_event_registered) {
        obs_frontend_add_event_callback(phase51_frontend_event, nullptr);
        phase51_frontend_event_registered = true;
    }
    load_phase51_hotkeys_from_profile();
    blog(LOG_INFO,
         "[ArZoom] P5.1 Spotlight hotkeys registered with profile persistence");
    return all_registered;
}

void arzoom_unregister_spotlight_hotkeys()
{
    save_phase51_hotkeys_to_profile();
    if (phase51_frontend_event_registered) {
        obs_frontend_remove_event_callback(phase51_frontend_event, nullptr);
        phase51_frontend_event_registered = false;
    }
    for (size_t i = 0; i < kPhase51HotkeyCount; ++i) {
        if (phase51_hotkeys[i] != OBS_INVALID_HOTKEY_ID) {
            obs_hotkey_unregister(phase51_hotkeys[i]);
            phase51_hotkeys[i] = OBS_INVALID_HOTKEY_ID;
        }
        phase51_hotkey_down[i].store(false, std::memory_order_release);
    }
}
