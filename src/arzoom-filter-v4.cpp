#include "arzoom-presenter-controls.hpp"

#include <array>
#include <atomic>
#include <new>
#include <string>
#include <vector>

/*
 * Phase 3 layers presenter controls around the frozen v0.3.1 runtime.
 * Phase 2 remains responsible for the accepted one-pass click visualization;
 * Phase 1 remains the Smart Zone camera implementation. P3 owns only control
 * intent, momentary overview transforms, hotkeys and deterministic target
 * selection.
 */
#include "arzoom-filter-v3.cpp"

namespace {

constexpr float kPresenterZoomStep = 0.25f;

enum class PresenterHotkey : size_t {
    HoldZoom = 0,
    FreezeCamera,
    ToggleFollow,
    ZoomIn,
    ZoomOut,
    Reset,
    OverviewPeek,
    Count,
};

constexpr size_t kPresenterHotkeyCount =
    static_cast<size_t>(PresenterHotkey::Count);

struct PresenterHotkeyDefinition {
    const char *config_name;
    const char *locale_key;
};

constexpr std::array<PresenterHotkeyDefinition, kPresenterHotkeyCount>
    presenter_hotkey_definitions{{
        {"arzoom.hold_zoom", "ArZoom.Hotkey.HoldZoom"},
        {"arzoom.freeze_camera", "ArZoom.Hotkey.FreezeCamera"},
        {"arzoom.toggle_follow", "ArZoom.Hotkey.ToggleFollow"},
        {"arzoom.zoom_in", "ArZoom.Hotkey.ZoomIn"},
        {"arzoom.zoom_out", "ArZoom.Hotkey.ZoomOut"},
        {"arzoom.reset", "ArZoom.Hotkey.Reset"},
        {"arzoom.overview_peek", "ArZoom.Hotkey.OverviewPeek"},
    }};

std::array<obs_hotkey_id, kPresenterHotkeyCount> presenter_hotkeys{{
    OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID,
    OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID,
    OBS_INVALID_HOTKEY_ID,
}};
std::array<std::atomic<bool>, kPresenterHotkeyCount> presenter_hotkey_down{};
bool presenter_frontend_event_registered = false;

struct Phase3Filter {
    Phase2Filter *phase2 = nullptr;

    /* Hotkey thread -> video tick communication is deliberately atomic and
     * scalar only. Camera geometry / monitor descriptors are never read from a
     * hotkey callback. */
    std::atomic<bool> hold_zoom{false};
    std::atomic<bool> freeze_camera{false};
    std::atomic<bool> follow_enabled{true};
    std::atomic<bool> overview_requested{false};

    arzoom::OverviewPeekController overview;

    /* Video-tick-owned Smart Follow resume state. */
    bool last_follow_enabled = true;
    bool follow_resume_seed_pending = false;
    bool follow_resume_anchor_valid = false;
    arzoom::Vec2 follow_resume_anchor{0.5f, 0.5f};
};

std::mutex presenter_registry_mutex;
std::vector<Phase3Filter *> presenter_registry;

ArZoomFilter *phase1_filter(Phase3Filter *filter)
{
    return filter && filter->phase2 ? filter->phase2->phase1 : nullptr;
}

bool presenter_filter_enabled(Phase3Filter *filter)
{
    ArZoomFilter *phase1 = phase1_filter(filter);
    return phase1 && phase1->enabled.load(std::memory_order_acquire);
}

void register_presenter_instance(Phase3Filter *filter)
{
    std::lock_guard<std::mutex> lock(presenter_registry_mutex);
    presenter_registry.push_back(filter);
}

void unregister_presenter_instance(Phase3Filter *filter)
{
    std::lock_guard<std::mutex> lock(presenter_registry_mutex);
    presenter_registry.erase(
        std::remove(presenter_registry.begin(), presenter_registry.end(), filter),
        presenter_registry.end());
}

template<typename Fn>
void for_presenter_targets(Fn &&fn)
{
    std::lock_guard<std::mutex> lock(presenter_registry_mutex);

    bool has_showing = false;
    for (Phase3Filter *filter : presenter_registry) {
        ArZoomFilter *phase1 = phase1_filter(filter);
        if (!phase1 || !presenter_filter_enabled(filter))
            continue;
        if (obs_source_showing(phase1->context)) {
            has_showing = true;
            break;
        }
    }

    for (Phase3Filter *filter : presenter_registry) {
        ArZoomFilter *phase1 = phase1_filter(filter);
        if (!phase1 || !presenter_filter_enabled(filter))
            continue;
        if (has_showing && !obs_source_showing(phase1->context))
            continue;
        fn(filter);
    }
}

template<typename Fn>
void for_all_presenter_instances(Fn &&fn)
{
    std::lock_guard<std::mutex> lock(presenter_registry_mutex);
    for (Phase3Filter *filter : presenter_registry) {
        if (filter)
            fn(filter);
    }
}

bool normalized_cursor_for_filter(ArZoomFilter *phase1, arzoom::Vec2 &cursor)
{
    if (!phase1 || !phase1->monitor_valid)
        return false;

    long cursor_x = 0;
    long cursor_y = 0;
    if (!get_cursor_position(cursor_x, cursor_y) ||
        !cursor_in_monitor(phase1->monitor, cursor_x, cursor_y)) {
        return false;
    }

    cursor = cursor_normalized(phase1->monitor, cursor_x, cursor_y);
    return true;
}

bool pressed_once(PresenterHotkey hotkey, bool pressed)
{
    const size_t index = static_cast<size_t>(hotkey);
    if (!pressed) {
        presenter_hotkey_down[index].store(false, std::memory_order_release);
        return false;
    }
    return !presenter_hotkey_down[index].exchange(
        true, std::memory_order_acq_rel);
}

void hold_zoom_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    const size_t index = static_cast<size_t>(PresenterHotkey::HoldZoom);
    if (pressed) {
        if (presenter_hotkey_down[index].exchange(
                true, std::memory_order_acq_rel))
            return;
        for_presenter_targets([](Phase3Filter *filter) {
            filter->hold_zoom.store(true, std::memory_order_release);
        });
        return;
    }

    presenter_hotkey_down[index].store(false, std::memory_order_release);
    /* Release is intentionally global: if the OBS scene changed while the key
     * was held, no hidden instance can remain stuck in a momentary zoom. */
    for_all_presenter_instances([](Phase3Filter *filter) {
        filter->hold_zoom.store(false, std::memory_order_release);
    });
}

void freeze_camera_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed_once(PresenterHotkey::FreezeCamera, pressed))
        return;
    for_presenter_targets([](Phase3Filter *filter) {
        const bool current =
            filter->freeze_camera.load(std::memory_order_acquire);
        filter->freeze_camera.store(!current, std::memory_order_release);
    });
}

void toggle_follow_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed_once(PresenterHotkey::ToggleFollow, pressed))
        return;

    /* Do not inspect camera or monitor state here. The video tick detects this
     * atomic transition and owns all resume geometry. */
    for_presenter_targets([](Phase3Filter *filter) {
        const bool current =
            filter->follow_enabled.load(std::memory_order_acquire);
        filter->follow_enabled.store(!current, std::memory_order_release);
    });
}

void queue_zoom_setting_sync(Phase3Filter *filter, float zoom)
{
    QWidget *main_window =
        static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!main_window)
        return;

    QMetaObject::invokeMethod(
        main_window,
        [filter, zoom]() {
            std::lock_guard<std::mutex> lock(presenter_registry_mutex);
            const auto found = std::find(
                presenter_registry.begin(), presenter_registry.end(), filter);
            if (found == presenter_registry.end())
                return;

            ArZoomFilter *phase1 = phase1_filter(filter);
            if (!phase1 || !phase1->context)
                return;

            obs_data_t *settings = obs_source_get_settings(phase1->context);
            if (!settings)
                return;
            obs_data_set_double(settings, SETTING_ZOOM, zoom);
            obs_source_update(phase1->context, settings);
            obs_data_release(settings);
            obs_source_update_properties(phase1->context);
        },
        Qt::QueuedConnection);
}

void adjust_zoom_for_filter(Phase3Filter *filter, float delta)
{
    ArZoomFilter *phase1 = phase1_filter(filter);
    if (!phase1)
        return;

    float current = phase1->configured_zoom.load(std::memory_order_acquire);
    float next = current;
    do {
        next = arzoom::presenter_zoom_step(current, delta);
        if (std::fabs(next - current) < 0.0001f)
            return;
    } while (!phase1->configured_zoom.compare_exchange_weak(
        current, next, std::memory_order_acq_rel,
        std::memory_order_acquire));

    queue_zoom_setting_sync(filter, next);
}

void zoom_in_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed_once(PresenterHotkey::ZoomIn, pressed))
        return;
    for_presenter_targets([](Phase3Filter *filter) {
        adjust_zoom_for_filter(filter, kPresenterZoomStep);
    });
}

void zoom_out_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed_once(PresenterHotkey::ZoomOut, pressed))
        return;
    for_presenter_targets([](Phase3Filter *filter) {
        adjust_zoom_for_filter(filter, -kPresenterZoomStep);
    });
}

void reset_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed_once(PresenterHotkey::Reset, pressed))
        return;

    for_presenter_targets([](Phase3Filter *filter) {
        ArZoomFilter *phase1 = phase1_filter(filter);
        if (phase1)
            phase1->requested_zoom.store(false, std::memory_order_release);
        filter->hold_zoom.store(false, std::memory_order_release);
        filter->freeze_camera.store(false, std::memory_order_release);
        filter->overview_requested.store(false, std::memory_order_release);
    });
}

void overview_peek_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    const size_t index = static_cast<size_t>(PresenterHotkey::OverviewPeek);
    if (pressed) {
        if (presenter_hotkey_down[index].exchange(
                true, std::memory_order_acq_rel))
            return;
        for_presenter_targets([](Phase3Filter *filter) {
            filter->overview_requested.store(true, std::memory_order_release);
        });
        return;
    }

    presenter_hotkey_down[index].store(false, std::memory_order_release);
    for_all_presenter_instances([](Phase3Filter *filter) {
        filter->overview_requested.store(false, std::memory_order_release);
    });
}

bool save_presenter_hotkeys_to_profile()
{
    config_t *config = obs_frontend_get_profile_config();
    if (!config)
        return false;

    for (size_t i = 0; i < kPresenterHotkeyCount; ++i) {
        const obs_hotkey_id id = presenter_hotkeys[i];
        if (id == OBS_INVALID_HOTKEY_ID)
            continue;

        obs_data_array_t *bindings = obs_hotkey_save(id);
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
                          presenter_hotkey_definitions[i].config_name,
                          json ? json : "");

        if (bindings)
            obs_data_array_release(bindings);
        obs_data_release(wrapper);
    }

    const int result = config_save_safe(config, "tmp", nullptr);
    if (result != CONFIG_SUCCESS) {
        blog(LOG_WARNING,
             "[ArZoom] Failed to persist presenter hotkeys to the OBS profile");
        return false;
    }
    return true;
}

bool load_presenter_hotkeys_from_profile()
{
    config_t *config = obs_frontend_get_profile_config();
    if (!config)
        return false;

    for (size_t i = 0; i < kPresenterHotkeyCount; ++i) {
        const obs_hotkey_id id = presenter_hotkeys[i];
        if (id == OBS_INVALID_HOTKEY_ID)
            continue;

        obs_hotkey_load_bindings(id, nullptr, 0);
        const char *json = config_get_string(
            config, HOTKEY_CONFIG_SECTION,
            presenter_hotkey_definitions[i].config_name);
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

size_t configured_presenter_hotkey_count()
{
    size_t count = hotkey_has_binding() ? 1u : 0u; /* Toggle Zoom from P1. */
    for (obs_hotkey_id id : presenter_hotkeys) {
        if (id == OBS_INVALID_HOTKEY_ID)
            continue;
        obs_data_array_t *bindings = obs_hotkey_save(id);
        if (bindings && obs_data_array_count(bindings) > 0)
            ++count;
        if (bindings)
            obs_data_array_release(bindings);
    }
    return count;
}

void presenter_frontend_event(enum obs_frontend_event event, void *)
{
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
    case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
        load_presenter_hotkeys_from_profile();
        refresh_filter_properties();
        break;
    case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
    case OBS_FRONTEND_EVENT_EXIT:
        save_presenter_hotkeys_to_profile();
        break;
    default:
        break;
    }
}

void update_follow_transition(Phase3Filter *filter, bool follow_now)
{
    if (filter->last_follow_enabled == follow_now)
        return;

    ArZoomFilter *phase1 = phase1_filter(filter);
    if (!follow_now) {
        arzoom::Vec2 cursor;
        filter->follow_resume_anchor_valid =
            normalized_cursor_for_filter(phase1, cursor);
        if (filter->follow_resume_anchor_valid)
            filter->follow_resume_anchor = cursor;
        filter->follow_resume_seed_pending = false;
    } else {
        filter->follow_resume_seed_pending =
            filter->follow_resume_anchor_valid;
    }
    filter->last_follow_enabled = follow_now;
}

arzoom::CameraInput make_phase3_camera_input(Phase3Filter *filter, float dt,
                                             bool wants_zoom)
{
    ArZoomFilter *phase1 = phase1_filter(filter);
    arzoom::CameraInput input;
    input.dt = dt;
    input.zoom_requested = wants_zoom;
    input.configured_zoom =
        phase1->configured_zoom.load(std::memory_order_acquire);
    input.anchor = {
        phase1->anchor_x.load(std::memory_order_acquire),
        phase1->anchor_y.load(std::memory_order_acquire),
    };
    input.safe_zone = phase1->safe_zone.load(std::memory_order_acquire);
    input.follow_policy = camera_follow_policy(
        static_cast<FollowMode>(
            phase1->follow_mode.load(std::memory_order_acquire)));
    input.motion_style = camera_motion_style(
        static_cast<MovementStyle>(
            phase1->movement_style.load(std::memory_order_acquire)));

    if (!filter->follow_enabled.load(std::memory_order_acquire))
        return input;

    if (filter->follow_resume_seed_pending) {
        filter->follow_resume_seed_pending = false;
        if (filter->follow_resume_anchor_valid) {
            input.cursor = filter->follow_resume_anchor;
            input.cursor_valid = true;
        }
        return input;
    }

    if (input.follow_policy != arzoom::CameraFollowPolicy::Fixed &&
        phase1->monitor_valid) {
        arzoom::Vec2 cursor;
        if (normalized_cursor_for_filter(phase1, cursor)) {
            input.cursor = cursor;
            input.cursor_valid = true;
        }
    }
    return input;
}

void step_overview(Phase3Filter *filter, float dt, bool wants_zoom)
{
    ArZoomFilter *phase1 = phase1_filter(filter);
    if (!phase1)
        return;

    const bool requested =
        filter->overview_requested.load(std::memory_order_acquire);

    if (requested && !filter->overview.active() && wants_zoom &&
        phase1->current_zoom > 1.0005f) {
        filter->overview.begin(phase1->current_center,
                               phase1->current_zoom);
    }

    if (!filter->overview.active())
        return;

    if (!wants_zoom) {
        if (filter->overview.phase() != arzoom::OverviewPhase::CancelToOverview)
            filter->overview.cancel_to_overview(
                phase1->current_center, phase1->current_zoom);
    } else if (!requested &&
               filter->overview.phase() != arzoom::OverviewPhase::ToShot) {
        filter->overview.release(phase1->current_center,
                                 phase1->current_zoom);
    }

    const auto style = camera_motion_style(
        static_cast<MovementStyle>(
            phase1->movement_style.load(std::memory_order_acquire)));
    const arzoom::CameraProfile profile = arzoom::camera_profile(style);
    const float out_seconds = std::clamp(
        profile.zoom_out_seconds * 0.62f, 0.24f, 0.42f);
    const float back_seconds = std::clamp(
        profile.zoom_in_seconds * 0.72f, 0.24f, 0.40f);

    const arzoom::OverviewOutput output =
        filter->overview.step(dt, out_seconds, back_seconds);
    phase1->current_center = output.center;
    phase1->current_zoom = output.zoom;

    if (output.cancelled) {
        phase1->camera.reset();
        phase1->current_center = {0.5f, 0.5f};
        phase1->current_zoom = 1.0f;
    }
}

void phase3_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    ArZoomFilter *phase1 = phase1_filter(filter);
    if (!filter || !phase1)
        return;

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    const bool enabled = phase1->enabled.load(std::memory_order_acquire);
    const bool wants_zoom = enabled && arzoom::presenter_zoom_requested(
        phase1->requested_zoom.load(std::memory_order_acquire),
        filter->hold_zoom.load(std::memory_order_acquire));

    const bool zoom_active = wants_zoom || phase1->current_zoom > 1.001f;
    refresh_monitor_if_needed(phase1, dt, zoom_active);

    const bool follow_now =
        filter->follow_enabled.load(std::memory_order_acquire);
    update_follow_transition(filter, follow_now);

    step_overview(filter, dt, wants_zoom);
    if (filter->overview.active()) {
        capture_clicks(filter->phase2, dt);
        return;
    }

    /* Exact freeze pauses the SmartCamera state itself. This preserves both
     * center and zoom, even if Freeze is pressed during an activation/follow
     * transition. Reset/toggle-off still wins because wants_zoom becomes false. */
    if (filter->freeze_camera.load(std::memory_order_acquire) && wants_zoom) {
        capture_clicks(filter->phase2, dt);
        return;
    }

    const arzoom::CameraInput input =
        make_phase3_camera_input(filter, dt, wants_zoom);
    const arzoom::CameraOutput output = phase1->camera.step(input);
    phase1->current_zoom = output.zoom;
    phase1->current_center = output.center;

    capture_clicks(filter->phase2, dt);
}

void phase3_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    if (!filter)
        return;
    phase2_update(filter->phase2, settings);
}

void phase3_defaults(obs_data_t *settings)
{
    phase2_defaults(settings);
}

obs_properties_t *phase3_properties(void *data)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    obs_properties_t *props = phase2_properties(filter ? filter->phase2 : nullptr);

    obs_properties_t *presenter = obs_properties_create();
    obs_property_t *info = obs_properties_add_text(
        presenter, "presenter_controls_info",
        obs_module_text("ArZoom.PresenterControls.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    const size_t configured = configured_presenter_hotkey_count();
    const char *status_key = configured >= 8
                                 ? "ArZoom.PresenterControls.Configured"
                                 : "ArZoom.PresenterControls.Configure";
    obs_property_t *status = obs_properties_add_text(
        presenter, "presenter_hotkey_status",
        obs_module_text(status_key), OBS_TEXT_INFO);
    obs_property_text_set_info_type(
        status, configured >= 8 ? OBS_TEXT_INFO_NORMAL : OBS_TEXT_INFO_WARNING);
    obs_property_text_set_info_word_wrap(status, true);

    obs_properties_add_button(
        presenter, "presenter_open_hotkeys",
        obs_module_text("ArZoom.OpenHotkeys"), open_hotkeys_clicked);
    obs_properties_add_group(
        props, "presenter_controls",
        obs_module_text("ArZoom.PresenterControls.Group"),
        OBS_GROUP_NORMAL, presenter);
    return props;
}

void phase3_deactivate(void *data)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    if (!filter)
        return;
    phase2_deactivate(filter->phase2);
    filter->hold_zoom.store(false, std::memory_order_release);
    filter->freeze_camera.store(false, std::memory_order_release);
    filter->overview_requested.store(false, std::memory_order_release);
    filter->overview.reset();
    filter->follow_resume_seed_pending = false;
    filter->follow_resume_anchor_valid = false;
    filter->last_follow_enabled =
        filter->follow_enabled.load(std::memory_order_acquire);
}

void phase3_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    if (!filter)
        return;
    phase2_render(filter->phase2, effect);
}

void phase3_destroy(void *data)
{
    auto *filter = static_cast<Phase3Filter *>(data);
    if (!filter)
        return;
    unregister_presenter_instance(filter);
    phase2_destroy(filter->phase2);
    delete filter;
}

void *phase3_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase2 = static_cast<Phase2Filter *>(
        phase2_create(settings, context));
    if (!phase2)
        return nullptr;

    auto *filter = new (std::nothrow) Phase3Filter();
    if (!filter) {
        phase2_destroy(phase2);
        return nullptr;
    }
    filter->phase2 = phase2;
    filter->last_follow_enabled =
        filter->follow_enabled.load(std::memory_order_acquire);
    register_presenter_instance(filter);
    blog(LOG_INFO, "[ArZoom] Phase 3 presenter controls ready");
    return filter;
}

/* Runs after the Phase 2 initializer in the included translation unit. */
struct Phase3SourceInfoOverride {
    Phase3SourceInfoOverride()
    {
        arzoom_filter_info.create = phase3_create;
        arzoom_filter_info.destroy = phase3_destroy;
        arzoom_filter_info.video_tick = phase3_tick;
        arzoom_filter_info.video_render = phase3_render;
        arzoom_filter_info.update = phase3_update;
        arzoom_filter_info.get_properties = phase3_properties;
        arzoom_filter_info.get_defaults = phase3_defaults;
        arzoom_filter_info.deactivate = phase3_deactivate;
    }
};

Phase3SourceInfoOverride phase3_source_info_override;

} // namespace

bool arzoom_register_presenter_hotkeys()
{
    static const std::array<obs_hotkey_func, kPresenterHotkeyCount> callbacks{{
        hold_zoom_hotkey,
        freeze_camera_hotkey,
        toggle_follow_hotkey,
        zoom_in_hotkey,
        zoom_out_hotkey,
        reset_hotkey,
        overview_peek_hotkey,
    }};

    bool all_registered = true;
    for (size_t i = 0; i < kPresenterHotkeyCount; ++i) {
        if (presenter_hotkeys[i] != OBS_INVALID_HOTKEY_ID)
            continue;
        presenter_hotkeys[i] = obs_hotkey_register_frontend(
            presenter_hotkey_definitions[i].config_name,
            obs_module_text(presenter_hotkey_definitions[i].locale_key),
            callbacks[i], nullptr);
        if (presenter_hotkeys[i] == OBS_INVALID_HOTKEY_ID) {
            all_registered = false;
            blog(LOG_ERROR, "[ArZoom] Failed to register presenter hotkey: %s",
                 presenter_hotkey_definitions[i].config_name);
        }
    }

    if (!presenter_frontend_event_registered) {
        obs_frontend_add_event_callback(presenter_frontend_event, nullptr);
        presenter_frontend_event_registered = true;
    }
    load_presenter_hotkeys_from_profile();
    blog(LOG_INFO,
         "[ArZoom] Presenter hotkeys registered with profile persistence");
    return all_registered;
}

void arzoom_unregister_presenter_hotkeys()
{
    save_presenter_hotkeys_to_profile();
    if (presenter_frontend_event_registered) {
        obs_frontend_remove_event_callback(presenter_frontend_event, nullptr);
        presenter_frontend_event_registered = false;
    }

    for (size_t i = 0; i < kPresenterHotkeyCount; ++i) {
        if (presenter_hotkeys[i] != OBS_INVALID_HOTKEY_ID) {
            obs_hotkey_unregister(presenter_hotkeys[i]);
            presenter_hotkeys[i] = OBS_INVALID_HOTKEY_ID;
        }
        presenter_hotkey_down[i].store(false, std::memory_order_release);
    }
}
