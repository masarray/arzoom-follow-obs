#include <obs-module.h>
#include <graphics/vec2.h>

#include "arzoom-math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#define SETTING_ENABLED "enabled"
#define SETTING_ZOOM "zoom_amount"
#define SETTING_FOLLOW_MODE "follow_mode"
#define SETTING_MOVEMENT "movement"
#define SETTING_SAFE_ZONE "safe_zone"
#define SETTING_ANCHOR_X "anchor_x"
#define SETTING_ANCHOR_Y "anchor_y"
#define SETTING_TARGET_MONITOR "target_monitor"
#define SETTING_RESET_HIDDEN "reset_when_hidden"

#define FOLLOW_SMART "smart"
#define FOLLOW_CENTERED "centered"
#define FOLLOW_FIXED "fixed"

#define MOVEMENT_SMOOTH "smooth"
#define MOVEMENT_BALANCED "balanced"
#define MOVEMENT_FAST "fast"

#define MONITOR_AUTO "auto"

namespace {

enum class FollowMode {
    Smart,
    Centered,
    Fixed,
};

enum class MovementStyle {
    Smooth,
    Balanced,
    Fast,
};

struct MovementProfile {
    float zoom_in_seconds;
    float zoom_out_seconds;
    float pan_seconds;
    float max_output_speed;
};

struct MonitorDescriptor {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
    bool primary = false;
    std::string device_name;
    std::string device_id;
    std::string label;

    long width() const { return right - left; }
    long height() const { return bottom - top; }
    bool valid() const { return width() > 0 && height() > 0; }
};

#ifdef _WIN32

std::string wide_to_utf8(const wchar_t *text)
{
    if (!text || !*text)
        return {};

    const int size =
        WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};

    std::string output(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), size, nullptr,
                        nullptr);
    output.pop_back();
    return output;
}

BOOL CALLBACK enumerate_monitor_callback(HMONITOR handle, HDC, LPRECT rect,
                                         LPARAM param)
{
    auto *monitors =
        reinterpret_cast<std::vector<MonitorDescriptor> *>(param);

    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, &info))
        return TRUE;

    MonitorDescriptor monitor;
    monitor.left = rect->left;
    monitor.top = rect->top;
    monitor.right = rect->right;
    monitor.bottom = rect->bottom;
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitor.device_name = wide_to_utf8(info.szDevice);

    DISPLAY_DEVICEW display_device = {};
    display_device.cb = sizeof(display_device);
    if (EnumDisplayDevicesW(info.szDevice, 0, &display_device,
                            EDD_GET_DEVICE_INTERFACE_NAME)) {
        monitor.device_id = wide_to_utf8(display_device.DeviceID);
    }

    monitor.label = monitor.device_name + "  ·  " +
                    std::to_string(monitor.width()) + "x" +
                    std::to_string(monitor.height()) + "  @ " +
                    std::to_string(monitor.left) + "," +
                    std::to_string(monitor.top);
    if (monitor.primary)
        monitor.label += "  ·  Primary";

    monitors->push_back(std::move(monitor));
    return TRUE;
}

std::vector<MonitorDescriptor> enumerate_monitors()
{
    std::vector<MonitorDescriptor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, enumerate_monitor_callback,
                        reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

bool get_cursor_position(long &x, long &y)
{
    POINT point = {};
    if (!GetCursorPos(&point))
        return false;

    x = point.x;
    y = point.y;
    return true;
}

#else

std::vector<MonitorDescriptor> enumerate_monitors()
{
    return {};
}

bool get_cursor_position(long &, long &)
{
    return false;
}

#endif

FollowMode parse_follow_mode(const char *value)
{
    if (value && std::strcmp(value, FOLLOW_CENTERED) == 0)
        return FollowMode::Centered;
    if (value && std::strcmp(value, FOLLOW_FIXED) == 0)
        return FollowMode::Fixed;
    return FollowMode::Smart;
}

MovementStyle parse_movement(const char *value)
{
    if (value && std::strcmp(value, MOVEMENT_BALANCED) == 0)
        return MovementStyle::Balanced;
    if (value && std::strcmp(value, MOVEMENT_FAST) == 0)
        return MovementStyle::Fast;
    return MovementStyle::Smooth;
}

MovementProfile movement_profile(MovementStyle style)
{
    switch (style) {
    case MovementStyle::Fast:
        return {0.17f, 0.16f, 0.11f, 2.8f};
    case MovementStyle::Balanced:
        return {0.24f, 0.22f, 0.16f, 2.0f};
    case MovementStyle::Smooth:
    default:
        return {0.34f, 0.30f, 0.23f, 1.35f};
    }
}

struct ArZoomFilter {
    obs_source_t *context = nullptr;
    gs_effect_t *effect = nullptr;
    gs_eparam_t *zoom_param = nullptr;
    gs_eparam_t *center_param = nullptr;
    bool effect_ready = false;
    std::string runtime_error;

    std::atomic<bool> requested_zoom{false};
    std::atomic<bool> reset_requested{false};

    std::atomic<bool> enabled{true};
    std::atomic<float> configured_zoom{2.0f};
    std::atomic<int> follow_mode{static_cast<int>(FollowMode::Smart)};
    std::atomic<int> movement_style{static_cast<int>(MovementStyle::Smooth)};
    std::atomic<float> safe_zone{0.28f};
    std::atomic<float> anchor_x{0.50f};
    std::atomic<float> anchor_y{0.45f};
    std::atomic<bool> reset_when_hidden{true};

    std::mutex monitor_setting_mutex;
    std::string configured_monitor = MONITOR_AUTO;
    std::atomic<bool> monitor_dirty{true};

    MonitorDescriptor monitor;
    bool monitor_valid = false;
    bool monitor_warning_logged = false;
    bool cursor_capture_warning_logged = false;
    std::string last_logged_monitor;
    float monitor_refresh_elapsed = 0.0f;
    uint32_t last_source_width = 0;
    uint32_t last_source_height = 0;

    float current_zoom = 1.0f;
    arzoom::Vec2 current_center{0.5f, 0.5f};
    arzoom::Vec2 target_center{0.5f, 0.5f};
};

obs_hotkey_id global_toggle_hotkey = OBS_INVALID_HOTKEY_ID;
std::atomic<bool> global_hotkey_down{false};
std::mutex filter_registry_mutex;
std::vector<ArZoomFilter *> filter_registry;

void register_filter_instance(ArZoomFilter *filter)
{
    std::lock_guard<std::mutex> lock(filter_registry_mutex);
    filter_registry.push_back(filter);
}

void unregister_filter_instance(ArZoomFilter *filter)
{
    std::lock_guard<std::mutex> lock(filter_registry_mutex);
    filter_registry.erase(
        std::remove(filter_registry.begin(), filter_registry.end(), filter),
        filter_registry.end());
}

void global_hotkey_toggle(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed) {
        global_hotkey_down.store(false, std::memory_order_release);
        return;
    }

    if (global_hotkey_down.exchange(true, std::memory_order_acq_rel))
        return;

    std::lock_guard<std::mutex> lock(filter_registry_mutex);
    if (filter_registry.empty())
        return;

    bool has_showing_filter = false;
    bool any_showing_filter_zoomed = false;

    for (ArZoomFilter *filter : filter_registry) {
        if (!filter ||
            !filter->enabled.load(std::memory_order_acquire))
            continue;

        if (obs_source_showing(filter->context)) {
            has_showing_filter = true;
            if (filter->requested_zoom.load(std::memory_order_acquire))
                any_showing_filter_zoomed = true;
        }
    }

    bool any_enabled_filter_zoomed = false;
    if (!has_showing_filter) {
        for (ArZoomFilter *filter : filter_registry) {
            if (filter &&
                filter->enabled.load(std::memory_order_acquire) &&
                filter->requested_zoom.load(std::memory_order_acquire)) {
                any_enabled_filter_zoomed = true;
                break;
            }
        }
    }

    const bool next_zoom =
        has_showing_filter ? !any_showing_filter_zoomed
                           : !any_enabled_filter_zoomed;

    for (ArZoomFilter *filter : filter_registry) {
        if (!filter ||
            !filter->enabled.load(std::memory_order_acquire))
            continue;

        if (has_showing_filter && !obs_source_showing(filter->context))
            continue;

        filter->requested_zoom.store(next_zoom,
                                     std::memory_order_release);
    }
}

const char *filter_name(void *)
{
    return obs_module_text("ArZoom.FilterName");
}

bool monitor_matches_setting(const MonitorDescriptor &monitor,
                             const std::string &setting)
{
    return setting == monitor.device_name ||
           (!monitor.device_id.empty() && setting == monitor.device_id);
}

bool resolve_monitor(ArZoomFilter *filter)
{
    const auto monitors = enumerate_monitors();
    if (monitors.empty())
        return false;

    std::string configured;
    {
        std::lock_guard<std::mutex> lock(filter->monitor_setting_mutex);
        configured = filter->configured_monitor;
    }

    if (!configured.empty() && configured != MONITOR_AUTO) {
        const auto found = std::find_if(
            monitors.begin(), monitors.end(),
            [&](const MonitorDescriptor &candidate) {
                return monitor_matches_setting(candidate, configured);
            });
        if (found != monitors.end()) {
            filter->monitor = *found;
            return true;
        }
    }

    obs_source_t *target = obs_filter_get_target(filter->context);
    if (target) {
        obs_data_t *settings = obs_source_get_settings(target);
        if (settings) {
            if (obs_data_has_user_value(settings, "capture_cursor")) {
                const bool captures_cursor =
                    obs_data_get_bool(settings, "capture_cursor");
                if (!captures_cursor &&
                    !filter->cursor_capture_warning_logged) {
                    blog(LOG_WARNING,
                         "[ArZoom] Display Capture cursor is disabled. "
                         "Mouse follow still works, but viewers may not "
                         "see the pointer.");
                    filter->cursor_capture_warning_logged = true;
                } else if (captures_cursor) {
                    filter->cursor_capture_warning_logged = false;
                }
            }

            const char *monitor_id =
                obs_data_get_string(settings, "monitor_id");
            if (monitor_id && *monitor_id &&
                std::strcmp(monitor_id, "DUMMY") != 0) {
                const auto found = std::find_if(
                    monitors.begin(), monitors.end(),
                    [&](const MonitorDescriptor &candidate) {
                        return monitor_matches_setting(candidate, monitor_id);
                    });
                if (found != monitors.end()) {
                    filter->monitor = *found;
                    obs_data_release(settings);
                    return true;
                }
            }

            if (obs_data_has_user_value(settings, "monitor")) {
                const int monitor_index =
                    static_cast<int>(obs_data_get_int(settings, "monitor"));
                if (monitor_index >= 0 &&
                    static_cast<size_t>(monitor_index) < monitors.size()) {
                    filter->monitor =
                        monitors[static_cast<size_t>(monitor_index)];
                    obs_data_release(settings);
                    return true;
                }
            }

            obs_data_release(settings);
        }

        const uint32_t width = obs_source_get_width(target);
        const uint32_t height = obs_source_get_height(target);
        const MonitorDescriptor *size_match = nullptr;
        size_t size_match_count = 0;
        for (const auto &candidate : monitors) {
            if ((static_cast<uint32_t>(candidate.width()) == width &&
                 static_cast<uint32_t>(candidate.height()) == height) ||
                (static_cast<uint32_t>(candidate.width()) == height &&
                 static_cast<uint32_t>(candidate.height()) == width)) {
                size_match = &candidate;
                ++size_match_count;
            }
        }
        if (size_match_count == 1 && size_match) {
            filter->monitor = *size_match;
            return true;
        }
    }

    const auto primary =
        std::find_if(monitors.begin(), monitors.end(),
                     [](const MonitorDescriptor &candidate) {
                         return candidate.primary;
                     });
    filter->monitor =
        primary != monitors.end() ? *primary : monitors.front();
    return filter->monitor.valid();
}

void refresh_monitor_if_needed(ArZoomFilter *filter, float seconds,
                               bool zoom_active)
{
    filter->monitor_refresh_elapsed += seconds;

    obs_source_t *target = obs_filter_get_target(filter->context);
    const uint32_t width = target ? obs_source_get_width(target) : 0;
    const uint32_t height = target ? obs_source_get_height(target) : 0;
    const bool source_size_changed =
        width != filter->last_source_width ||
        height != filter->last_source_height;

    if (source_size_changed) {
        filter->last_source_width = width;
        filter->last_source_height = height;
        filter->monitor_dirty.store(true, std::memory_order_release);
    }

    if (zoom_active && filter->monitor_refresh_elapsed >= 5.0f)
        filter->monitor_dirty.store(true, std::memory_order_release);

    if (!filter->monitor_dirty.exchange(false,
                                        std::memory_order_acq_rel))
        return;

    filter->monitor_refresh_elapsed = 0.0f;
    filter->monitor_valid = resolve_monitor(filter);

    if (!filter->monitor_valid && !filter->monitor_warning_logged) {
        blog(LOG_WARNING,
             "[ArZoom] Target monitor could not be resolved. Zoom remains "
             "safe, but mouse follow is held until monitor mapping is ready.");
        filter->monitor_warning_logged = true;
    } else if (filter->monitor_valid) {
        filter->monitor_warning_logged = false;
        if (filter->last_logged_monitor != filter->monitor.label) {
            filter->last_logged_monitor = filter->monitor.label;
            blog(LOG_INFO, "[ArZoom] Monitor mapped: %s",
                 filter->monitor.label.c_str());
        }
    }
}

bool cursor_in_monitor(const MonitorDescriptor &monitor, long x, long y)
{
    return x >= monitor.left && x < monitor.right &&
           y >= monitor.top && y < monitor.bottom;
}

arzoom::Vec2 cursor_normalized(const MonitorDescriptor &monitor,
                              long x, long y)
{
    return {
        static_cast<float>(x - monitor.left) /
            static_cast<float>(monitor.width()),
        static_cast<float>(y - monitor.top) /
            static_cast<float>(monitor.height()),
    };
}

bool test_zoom_clicked(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<ArZoomFilter *>(data);
    if (!filter)
        return false;

    const bool current =
        filter->requested_zoom.load(std::memory_order_acquire);
    filter->requested_zoom.store(!current, std::memory_order_release);
    return false;
}

bool reset_clicked(obs_properties_t *, obs_property_t *, void *data)
{
    auto *filter = static_cast<ArZoomFilter *>(data);
    if (!filter)
        return false;

    filter->requested_zoom.store(false, std::memory_order_release);
    filter->reset_requested.store(true, std::memory_order_release);
    return false;
}

void update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<ArZoomFilter *>(data);

    filter->enabled.store(obs_data_get_bool(settings, SETTING_ENABLED),
                          std::memory_order_release);
    filter->configured_zoom.store(
        std::clamp(static_cast<float>(
                       obs_data_get_double(settings, SETTING_ZOOM)),
                   1.10f, 4.00f),
        std::memory_order_release);
    filter->follow_mode.store(
        static_cast<int>(
            parse_follow_mode(
                obs_data_get_string(settings, SETTING_FOLLOW_MODE))),
        std::memory_order_release);
    filter->movement_style.store(
        static_cast<int>(
            parse_movement(
                obs_data_get_string(settings, SETTING_MOVEMENT))),
        std::memory_order_release);
    filter->safe_zone.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_SAFE_ZONE)) /
                       100.0f,
                   0.10f, 0.60f),
        std::memory_order_release);
    filter->anchor_x.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_ANCHOR_X)) /
                       100.0f,
                   0.20f, 0.80f),
        std::memory_order_release);
    filter->anchor_y.store(
        std::clamp(static_cast<float>(
                       obs_data_get_int(settings, SETTING_ANCHOR_Y)) /
                       100.0f,
                   0.20f, 0.80f),
        std::memory_order_release);
    filter->reset_when_hidden.store(
        obs_data_get_bool(settings, SETTING_RESET_HIDDEN),
        std::memory_order_release);

    const char *target_monitor =
        obs_data_get_string(settings, SETTING_TARGET_MONITOR);
    {
        std::lock_guard<std::mutex> lock(
            filter->monitor_setting_mutex);
        filter->configured_monitor =
            (target_monitor && *target_monitor)
                ? target_monitor
                : MONITOR_AUTO;
    }
    filter->monitor_dirty.store(true, std::memory_order_release);

    if (!filter->enabled.load(std::memory_order_acquire))
        filter->requested_zoom.store(false,
                                     std::memory_order_release);
}

void defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, SETTING_ENABLED, true);
    obs_data_set_default_double(settings, SETTING_ZOOM, 2.00);
    obs_data_set_default_string(settings, SETTING_FOLLOW_MODE,
                                FOLLOW_SMART);
    obs_data_set_default_string(settings, SETTING_MOVEMENT,
                                MOVEMENT_SMOOTH);
    obs_data_set_default_int(settings, SETTING_SAFE_ZONE, 28);
    obs_data_set_default_int(settings, SETTING_ANCHOR_X, 50);
    obs_data_set_default_int(settings, SETTING_ANCHOR_Y, 45);
    obs_data_set_default_string(settings, SETTING_TARGET_MONITOR,
                                MONITOR_AUTO);
    obs_data_set_default_bool(settings, SETTING_RESET_HIDDEN, true);
}

obs_properties_t *properties(void *data)
{
    auto *filter = static_cast<ArZoomFilter *>(data);
    obs_properties_t *props = obs_properties_create();

    const char *status_text = obs_module_text("ArZoom.Status.Ready");
    enum obs_text_info_type status_type = OBS_TEXT_INFO_NORMAL;
    if (filter && !filter->effect_ready) {
        status_text = filter->runtime_error.empty()
                          ? obs_module_text("ArZoom.Status.ShaderUnavailable")
                          : filter->runtime_error.c_str();
        status_type = OBS_TEXT_INFO_ERROR;
    }
    obs_property_t *status = obs_properties_add_text(
        props, "runtime_status", status_text, OBS_TEXT_INFO);
    obs_property_text_set_info_type(status, status_type);
    obs_property_text_set_info_word_wrap(status, true);

    obs_properties_add_bool(props, SETTING_ENABLED,
                            obs_module_text("ArZoom.Enabled"));

    obs_property_t *zoom = obs_properties_add_float_slider(
        props, SETTING_ZOOM, obs_module_text("ArZoom.ZoomAmount"),
        1.10, 4.00, 0.05);
    obs_property_float_set_suffix(zoom, "x");

    obs_property_t *follow = obs_properties_add_list(
        props, SETTING_FOLLOW_MODE,
        obs_module_text("ArZoom.MouseFollow"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        follow, obs_module_text("Follow.Smart"), FOLLOW_SMART);
    obs_property_list_add_string(
        follow, obs_module_text("Follow.Centered"), FOLLOW_CENTERED);
    obs_property_list_add_string(
        follow, obs_module_text("Follow.Fixed"), FOLLOW_FIXED);

    obs_property_t *movement = obs_properties_add_list(
        props, SETTING_MOVEMENT,
        obs_module_text("ArZoom.Movement"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        movement, obs_module_text("Movement.Smooth"),
        MOVEMENT_SMOOTH);
    obs_property_list_add_string(
        movement, obs_module_text("Movement.Balanced"),
        MOVEMENT_BALANCED);
    obs_property_list_add_string(
        movement, obs_module_text("Movement.Fast"),
        MOVEMENT_FAST);

    obs_property_t *safe_zone = obs_properties_add_int_slider(
        props, SETTING_SAFE_ZONE,
        obs_module_text("ArZoom.SafeZone"), 10, 60, 1);
    obs_property_int_set_suffix(safe_zone, " %");

    obs_properties_add_text(
        props, "hotkey_info",
        obs_module_text("ArZoom.HotkeyInfo"), OBS_TEXT_INFO);

    if (filter) {
        obs_properties_add_button2(
            props, "test_zoom", obs_module_text("ArZoom.TestZoom"),
            test_zoom_clicked, filter);
        obs_properties_add_button2(
            props, "reset_zoom", obs_module_text("ArZoom.Reset"),
            reset_clicked, filter);
    }

    obs_properties_t *advanced = obs_properties_create();

    obs_property_t *monitors = obs_properties_add_list(
        advanced, SETTING_TARGET_MONITOR,
        obs_module_text("ArZoom.TargetMonitor"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        monitors, obs_module_text("Monitor.Auto"), MONITOR_AUTO);
    for (const auto &monitor : enumerate_monitors()) {
        const std::string value = !monitor.device_id.empty()
                                      ? monitor.device_id
                                      : monitor.device_name;
        obs_property_list_add_string(
            monitors, monitor.label.c_str(), value.c_str());
    }

    obs_property_t *anchor_x = obs_properties_add_int_slider(
        advanced, SETTING_ANCHOR_X,
        obs_module_text("ArZoom.AnchorX"), 20, 80, 1);
    obs_property_int_set_suffix(anchor_x, " %");

    obs_property_t *anchor_y = obs_properties_add_int_slider(
        advanced, SETTING_ANCHOR_Y,
        obs_module_text("ArZoom.AnchorY"), 20, 80, 1);
    obs_property_int_set_suffix(anchor_y, " %");

    obs_properties_add_bool(
        advanced, SETTING_RESET_HIDDEN,
        obs_module_text("ArZoom.ResetWhenHidden"));

    obs_properties_add_group(
        props, "advanced", obs_module_text("ArZoom.Advanced"),
        OBS_GROUP_NORMAL, advanced);

    return props;
}

void deactivate(void *data)
{
    auto *filter = static_cast<ArZoomFilter *>(data);
    if (filter->reset_when_hidden.load(std::memory_order_acquire)) {
        filter->requested_zoom.store(false,
                                     std::memory_order_release);
        filter->reset_requested.store(true,
                                      std::memory_order_release);
    }
}

void tick(void *data, float seconds)
{
    auto *filter = static_cast<ArZoomFilter *>(data);

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    const bool is_enabled =
        filter->enabled.load(std::memory_order_acquire);
    const bool wants_zoom =
        is_enabled &&
        filter->requested_zoom.load(std::memory_order_acquire);

    const float configured_zoom =
        filter->configured_zoom.load(std::memory_order_acquire);
    const float target_zoom = wants_zoom ? configured_zoom : 1.0f;

    const auto movement = static_cast<MovementStyle>(
        filter->movement_style.load(std::memory_order_acquire));
    const MovementProfile profile = movement_profile(movement);

    const bool zoom_active =
        wants_zoom || filter->current_zoom > 1.001f;
    refresh_monitor_if_needed(filter, dt, zoom_active);

    /*
     * Calculate the center target before changing zoom. When zoom is reducing,
     * pan is advanced first and the zoom level is not allowed to become wider
     * than the current center can safely support. This prevents the edge snap
     * common in uncoupled zoom/pan implementations.
     */
    if (!wants_zoom) {
        filter->target_center = {0.5f, 0.5f};
    } else {
        const auto follow = static_cast<FollowMode>(
            filter->follow_mode.load(std::memory_order_acquire));
        const arzoom::Vec2 anchor{
            filter->anchor_x.load(std::memory_order_acquire),
            filter->anchor_y.load(std::memory_order_acquire),
        };

        if (follow == FollowMode::Fixed) {
            filter->target_center = {0.5f, 0.5f};
        } else if (filter->monitor_valid) {
            long cursor_x = 0;
            long cursor_y = 0;
            if (get_cursor_position(cursor_x, cursor_y) &&
                cursor_in_monitor(filter->monitor, cursor_x,
                                  cursor_y)) {
                const arzoom::Vec2 cursor =
                    cursor_normalized(filter->monitor, cursor_x,
                                      cursor_y);
                const float follow_zoom =
                    std::max(configured_zoom, 1.01f);

                if (follow == FollowMode::Centered) {
                    filter->target_center =
                        arzoom::centered_target(
                            cursor, anchor, follow_zoom);
                } else {
                    filter->target_center =
                        arzoom::smart_follow_target(
                            cursor, filter->current_center, anchor,
                            filter->safe_zone.load(
                                std::memory_order_acquire),
                            follow_zoom);
                }
            }
        }
    }

    const bool zoom_increasing =
        target_zoom >= filter->current_zoom;

    if (zoom_increasing) {
        filter->current_zoom = arzoom::smooth_scalar(
            filter->current_zoom, target_zoom, dt,
            profile.zoom_in_seconds);

        filter->target_center =
            arzoom::clamp_center(
                filter->target_center,
                std::max(filter->current_zoom, 1.0f));
        filter->current_center = arzoom::smooth_center(
            filter->current_center, filter->target_center, dt,
            profile.pan_seconds, profile.max_output_speed,
            std::max(filter->current_zoom, 1.0f));
    } else {
        /*
         * Pan first while the old, narrower viewport is still valid.
         * Then permit zoom-out only as far as the new center allows.
         */
        filter->target_center =
            arzoom::clamp_center(
                filter->target_center,
                std::max(target_zoom, 1.0f));
        filter->current_center = arzoom::smooth_center(
            filter->current_center, filter->target_center, dt,
            profile.pan_seconds, profile.max_output_speed,
            std::max(filter->current_zoom, 1.0f));

        float next_zoom = arzoom::smooth_scalar(
            filter->current_zoom, target_zoom, dt,
            profile.zoom_out_seconds);
        const float edge_safe_zoom =
            arzoom::minimum_zoom_for_center(
                filter->current_center);
        filter->current_zoom =
            std::max(next_zoom, edge_safe_zoom);

        /* Numerical guard only; this should no longer create a visible jump. */
        filter->current_center =
            arzoom::clamp_center(
                filter->current_center,
                std::max(filter->current_zoom, 1.0f));
    }

    if (filter->reset_requested.exchange(
            false, std::memory_order_acq_rel) &&
        !wants_zoom) {
        filter->target_center = {0.5f, 0.5f};
    }

    if (!wants_zoom &&
        arzoom::nearly_equal(filter->current_zoom, 1.0f, 0.001f) &&
        arzoom::nearly_equal(filter->current_center,
                             {0.5f, 0.5f}, 0.001f)) {
        filter->current_zoom = 1.0f;
        filter->current_center = {0.5f, 0.5f};
        filter->target_center = {0.5f, 0.5f};
    }
}

void render(void *data, gs_effect_t *)
{
    auto *filter = static_cast<ArZoomFilter *>(data);

    if (!filter->effect_ready || !filter->effect ||
        !filter->enabled.load(std::memory_order_acquire) ||
        (filter->current_zoom <= 1.0005f &&
         arzoom::nearly_equal(filter->current_center,
                              {0.5f, 0.5f}, 0.0005f))) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (!obs_source_process_filter_begin(
            filter->context, GS_RGBA,
            OBS_NO_DIRECT_RENDERING)) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    gs_effect_set_float(filter->zoom_param,
                        std::max(filter->current_zoom, 1.0f));
    vec2 center;
    vec2_set(&center, filter->current_center.x,
             filter->current_center.y);
    gs_effect_set_vec2(filter->center_param, &center);

    obs_source_process_filter_end(
        filter->context, filter->effect, 0, 0);
}

void destroy(void *data)
{
    auto *filter = static_cast<ArZoomFilter *>(data);

    unregister_filter_instance(filter);

    obs_enter_graphics();
    if (filter->effect)
        gs_effect_destroy(filter->effect);
    obs_leave_graphics();

    delete filter;
}

void *create(obs_data_t *settings, obs_source_t *context)
{
    auto *filter = new (std::nothrow) ArZoomFilter();
    if (!filter)
        return nullptr;

    filter->context = context;
    register_filter_instance(filter);

    char *effect_path = obs_module_file("effects/arzoom.effect");
    if (!effect_path) {
        filter->runtime_error =
            obs_module_text("ArZoom.Status.EffectMissing");
        blog(LOG_ERROR,
             "[ArZoom] effects/arzoom.effect was not found. "
             "Filter remains in safe pass-through mode.");
    } else {
        char *effect_errors = nullptr;
        obs_enter_graphics();
        filter->effect =
            gs_effect_create_from_file(effect_path, &effect_errors);
        if (filter->effect) {
            filter->zoom_param =
                gs_effect_get_param_by_name(filter->effect, "zoom_amount");
            filter->center_param =
                gs_effect_get_param_by_name(filter->effect, "zoom_center");
        }
        obs_leave_graphics();

        if (effect_errors && *effect_errors) {
            blog(LOG_ERROR, "[ArZoom] Shader message: %s", effect_errors);
            filter->runtime_error =
                obs_module_text("ArZoom.Status.ShaderCompileFailed");
        }
        if (effect_errors)
            bfree(effect_errors);
        bfree(effect_path);

        filter->effect_ready = filter->effect && filter->zoom_param &&
                               filter->center_param;
        if (!filter->effect_ready) {
            if (filter->runtime_error.empty())
                filter->runtime_error =
                    obs_module_text("ArZoom.Status.ShaderUnavailable");

            blog(LOG_ERROR,
                 "[ArZoom] Zoom shader is unavailable. "
                 "Filter remains in safe pass-through mode; controls and "
                 "hotkey stay available for diagnosis.");
        } else {
            blog(LOG_INFO, "[ArZoom] Zoom shader ready");
        }
    }

    update(filter, settings);
    return filter;
}

} // namespace

bool arzoom_register_global_hotkey()
{
    if (global_toggle_hotkey != OBS_INVALID_HOTKEY_ID)
        return true;

    global_toggle_hotkey = obs_hotkey_register_frontend(
        "arzoom.toggle",
        obs_module_text("ArZoom.Hotkey.Toggle"),
        global_hotkey_toggle, nullptr);

    if (global_toggle_hotkey == OBS_INVALID_HOTKEY_ID) {
        blog(LOG_ERROR,
             "[ArZoom] Failed to register the global frontend hotkey");
        return false;
    }

    blog(LOG_INFO,
         "[ArZoom] Global frontend hotkey registered");
    return true;
}

void arzoom_unregister_global_hotkey()
{
    if (global_toggle_hotkey != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(global_toggle_hotkey);
        global_toggle_hotkey = OBS_INVALID_HOTKEY_ID;
    }

    global_hotkey_down.store(false, std::memory_order_release);
}

obs_source_info arzoom_filter_info = {};

namespace {
struct ArZoomSourceInfoInitializer {
    ArZoomSourceInfoInitializer()
    {
        arzoom_filter_info.id = "arzoom_filter";
        arzoom_filter_info.type = OBS_SOURCE_TYPE_FILTER;
        arzoom_filter_info.output_flags = OBS_SOURCE_VIDEO;
        arzoom_filter_info.get_name = filter_name;
        arzoom_filter_info.create = create;
        arzoom_filter_info.destroy = destroy;
        arzoom_filter_info.video_tick = tick;
        arzoom_filter_info.video_render = render;
        arzoom_filter_info.update = update;
        arzoom_filter_info.get_properties = properties;
        arzoom_filter_info.get_defaults = defaults;
        arzoom_filter_info.deactivate = deactivate;
        arzoom_filter_info.icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE;
    }
};

ArZoomSourceInfoInitializer arzoom_source_info_initializer;
} // namespace
