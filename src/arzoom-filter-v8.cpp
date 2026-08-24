#include "arzoom-filter-v7.cpp"
#include "arzoom-cursor-presets.hpp"

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>

#include <cmath>

#define SETTING_PRESENTATION_CURSOR_STYLE "presentation_cursor_style"
#define CURSOR_CUSTOM_GROUP "presentation_cursor_custom_group"

namespace {

constexpr int kBuiltinCursorFrameSize = 160;
constexpr int kBuiltinCursorColumns = 6;
constexpr int kBuiltinCursorRows = 5;

struct Phase352Filter {
    Phase351Filter *phase351 = nullptr;
    std::mutex style_mutex;
    std::string active_style = arzoom::kCursorStyleOff;
    std::string last_custom_path;
    bool last_custom_auto_key = true;
};

Phase35Filter *phase35_from_352(Phase352Filter *filter)
{
    return filter && filter->phase351 ? filter->phase351->phase35 : nullptr;
}

ArZoomFilter *phase1_from_352(Phase352Filter *filter)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    return phase35 ? phase1_filter35(phase35) : nullptr;
}

float click_press_amount(size_t frame_index, size_t frame_count)
{
    if (frame_index == 0 || frame_count <= 2)
        return 0.0f;
    const float t = static_cast<float>(frame_index - 1) /
                    static_cast<float>(frame_count - 2);
    if (t < 0.34f) {
        const float u = t / 0.34f;
        return 0.5f - 0.5f * std::cos(3.14159265f * u);
    }
    const float u = (t - 0.34f) / 0.66f;
    return 0.5f + 0.5f * std::cos(3.14159265f * u);
}

QPolygonF transformed_pointer(float press, float rotation_degrees)
{
    static const QPointF points[] = {
        {31.0, 17.0}, {126.0, 79.0}, {91.0, 91.0},
        {116.0, 139.0}, {97.0, 148.0}, {73.0, 101.0}, {46.0, 130.0},
    };
    const QPointF tip(31.0, 17.0);
    const float sx = 1.0f - 0.095f * press;
    const float sy = 1.0f - 0.075f * press;
    const float radians = rotation_degrees * press * 3.14159265f / 180.0f;
    const float ca = std::cos(radians);
    const float sa = std::sin(radians);

    QPolygonF polygon;
    for (const QPointF &point : points) {
        const float dx = static_cast<float>(point.x() - tip.x()) * sx;
        const float dy = static_cast<float>(point.y() - tip.y()) * sy;
        polygon << QPointF(
            tip.x() + dx * ca - dy * sa + 1.8f * press,
            tip.y() + dx * sa + dy * ca + 2.2f * press);
    }
    return polygon;
}

void draw_builtin_frame(QPainter &painter,
                        const arzoom::CursorPreset &preset,
                        size_t frame_index)
{
    const float press = click_press_amount(frame_index, preset.frame_count);
    float rotation = 0.0f;
    switch (preset.visual) {
    case arzoom::BuiltinCursorVisual::Prism: rotation = -1.7f; break;
    case arzoom::BuiltinCursorVisual::Outline: rotation = 1.0f; break;
    case arzoom::BuiltinCursorVisual::Azure: rotation = -0.8f; break;
    case arzoom::BuiltinCursorVisual::Orchid: rotation = 1.4f; break;
    }
    const QPolygonF polygon = transformed_pointer(press, rotation);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (preset.visual == arzoom::BuiltinCursorVisual::Prism) {
        QPolygonF shadow = polygon;
        for (QPointF &point : shadow)
            point += QPointF(-4.0 + 1.5 * press, 5.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 89, 176, 235));
        painter.drawPolygon(shadow);

        QLinearGradient fill(32, 18, 112, 146);
        fill.setColorAt(0.0, QColor(33, 205, 246));
        fill.setColorAt(1.0, QColor(21, 145, 231));
        painter.setBrush(fill);
        painter.setPen(QPen(QColor(0, 113, 208), 2.0,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolygon(polygon);

        QPolygonF inner;
        inner << polygon[0]
              << (polygon[0] * 0.16 + polygon[1] * 0.84)
              << (polygon[0] * 0.15 + polygon[2] * 0.85)
              << (polygon[0] * 0.30 + polygon[4] * 0.70);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(113, 231, 252, 115));
        painter.drawPolygon(inner);
        if (press > 0.15f) {
            painter.setBrush(QColor(227, 252, 255,
                                    static_cast<int>(135.0f * press)));
            painter.drawEllipse(QPointF(54.0 + 12.0 * press,
                                        49.0 + 12.0 * press),
                                3.0 + 2.0 * press,
                                3.0 + 2.0 * press);
        }
        return;
    }

    if (preset.visual == arzoom::BuiltinCursorVisual::Outline) {
        painter.setBrush(QColor(250, 251, 253, 245));
        painter.setPen(QPen(QColor(17, 20, 25), 5.0,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolygon(polygon);
        if (press > 0.05f) {
            painter.setPen(QPen(QColor(77, 84, 94,
                                      static_cast<int>(130.0f * press)),
                                2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(48, 38), QPointF(102, 74));
        }
        return;
    }

    if (preset.visual == arzoom::BuiltinCursorVisual::Azure) {
        QLinearGradient fill(32, 18, 112, 146);
        fill.setColorAt(0.0, QColor(223, 244, 255));
        fill.setColorAt(1.0, QColor(151, 211, 247));
        painter.setBrush(fill);
        painter.setPen(QPen(QColor(41, 134, 204), 3.0,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolygon(polygon);
        painter.setPen(QPen(QColor(255, 255, 255, 155), 3.0,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(43, 31), QPointF(98, 67));
        return;
    }

    QLinearGradient fill(32, 18, 112, 146);
    fill.setColorAt(0.0, QColor(143, 220, 252));
    fill.setColorAt(1.0, QColor(100, 188, 235));
    painter.setBrush(fill);
    painter.setPen(QPen(QColor(119, 79, 151), 4.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolygon(polygon);
    painter.setPen(QPen(QColor(119, 79, 151, 220), 4.0,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(51, 42), QPointF(65, 50));
    painter.drawLine(QPointF(73, 54), QPointF(101, 72));
    if (press > 0.08f) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(61, 230, 221,
                                static_cast<int>(170.0f * press)));
        painter.drawEllipse(QPointF(116.0 - 8.0 * press,
                                    91.0 + 6.0 * press),
                            4.0 + 2.0 * press,
                            4.0 + 2.0 * press);
    }
}

QImage build_builtin_atlas(const arzoom::CursorPreset &preset)
{
    QImage atlas(kBuiltinCursorFrameSize * kBuiltinCursorColumns,
                 kBuiltinCursorFrameSize * kBuiltinCursorRows,
                 QImage::Format_RGBA8888);
    if (atlas.isNull())
        return {};
    atlas.fill(Qt::transparent);

    QPainter atlas_painter(&atlas);
    atlas_painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (size_t i = 0; i < preset.frame_count; ++i) {
        QImage frame(kBuiltinCursorFrameSize, kBuiltinCursorFrameSize,
                     QImage::Format_RGBA8888);
        frame.fill(Qt::transparent);
        QPainter frame_painter(&frame);
        draw_builtin_frame(frame_painter, preset, i);
        frame_painter.end();

        const int x = static_cast<int>(i % kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        const int y = static_cast<int>(i / kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        atlas_painter.drawImage(x, y, frame);
    }
    atlas_painter.end();
    return atlas;
}

void clear_builtin_texture(Phase352Filter *filter, const std::string &error)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    if (!phase35)
        return;
    gs_texture_t *old_texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(phase35->asset_mutex);
        old_texture = phase35->atlas_texture;
        phase35->atlas_texture = nullptr;
        phase35->frame_width = 0;
        phase35->frame_height = 0;
        phase35->atlas_columns = 1;
        phase35->atlas_rows = 1;
        phase35->frame_count = 0;
        phase35->asset_error = error;
    }
    destroy_cursor_texture(old_texture);
    phase35->current_frame.store(0, std::memory_order_release);
    queue_playback_configuration(filter->phase351, 1, kCursorPlaySeconds);
}

bool load_builtin_preset(Phase352Filter *filter,
                         const arzoom::CursorPreset &preset)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    if (!phase35 || !arzoom::cursor_preset_is_valid(preset))
        return false;

    QImage atlas = build_builtin_atlas(preset);
    if (atlas.isNull()) {
        clear_builtin_texture(filter,
                              "Could not allocate built-in cursor atlas.");
        return false;
    }

    const uint8_t *pixels = atlas.constBits();
    gs_texture_t *texture = nullptr;
    obs_enter_graphics();
    texture = gs_texture_create(
        static_cast<uint32_t>(atlas.width()),
        static_cast<uint32_t>(atlas.height()),
        GS_RGBA, 1, &pixels, 0);
    obs_leave_graphics();
    if (!texture) {
        clear_builtin_texture(filter,
                              "GPU texture creation failed for built-in cursor.");
        return false;
    }

    gs_texture_t *old_texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(phase35->asset_mutex);
        old_texture = phase35->atlas_texture;
        phase35->atlas_texture = texture;
        phase35->frame_width = kBuiltinCursorFrameSize;
        phase35->frame_height = kBuiltinCursorFrameSize;
        phase35->atlas_columns = kBuiltinCursorColumns;
        phase35->atlas_rows = kBuiltinCursorRows;
        phase35->frame_count = preset.frame_count;
        phase35->asset_error.clear();
    }
    destroy_cursor_texture(old_texture);

    phase35->hotspot_x.store(preset.hotspot_x, std::memory_order_release);
    phase35->hotspot_y.store(preset.hotspot_y, std::memory_order_release);
    phase35->current_frame.store(0, std::memory_order_release);
    queue_playback_configuration(filter->phase351,
                                 preset.frame_count, preset.play_seconds);
    blog(LOG_INFO,
         "[ArZoom] Built-in Presentation Cursor ready: %s (%zu frames)",
         preset.id, preset.frame_count);
    return true;
}

bool native_cursor_probably_enabled(ArZoomFilter *phase1)
{
    if (!phase1)
        return false;
    obs_source_t *target = obs_filter_get_target(phase1->context);
    if (!target)
        return false;
    obs_data_t *settings = obs_source_get_settings(target);
    if (!settings)
        return false;
    bool captures = false;
    if (obs_data_has_user_value(settings, "capture_cursor")) {
        captures = obs_data_get_bool(settings, "capture_cursor");
    } else {
        const char *source_id = obs_source_get_id(target);
        captures = source_id &&
                   (std::strstr(source_id, "monitor") != nullptr ||
                    std::strstr(source_id, "display") != nullptr);
    }
    obs_data_release(settings);
    return captures;
}

std::string resolved_style(obs_data_t *settings)
{
    const char *value =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_STYLE);
    std::string style = value ? value : "";
    if (!obs_data_has_user_value(settings,
                                 SETTING_PRESENTATION_CURSOR_STYLE)) {
        const bool legacy_enabled =
            obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR);
        const char *legacy_path =
            obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
        if (legacy_enabled) {
            style = legacy_path && *legacy_path
                        ? arzoom::kCursorStyleCustom : "prism";
            obs_data_set_string(settings,
                                SETTING_PRESENTATION_CURSOR_STYLE,
                                style.c_str());
        }
    }
    if (arzoom::cursor_style_is_off(style.c_str()) ||
        arzoom::cursor_style_is_custom(style.c_str()) ||
        arzoom::find_cursor_preset(style.c_str()))
        return style.empty() ? arzoom::kCursorStyleOff : style;
    return arzoom::kCursorStyleOff;
}

void set_runtime_enabled(Phase352Filter *filter, bool enabled)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    if (!phase35)
        return;
    phase35->cursor_enabled.store(enabled, std::memory_order_release);
    if (Phase2Filter *phase2 = phase2_filter(phase35))
        phase2->click_capture_for_cursor.store(enabled,
                                                std::memory_order_release);
    if (!enabled) {
        phase35->playback.reset();
        phase35->current_frame.store(0, std::memory_order_release);
        phase35->cursor_position_valid.store(false,
                                              std::memory_order_release);
    }
}

void phase352_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    if (!filter || !filter->phase351)
        return;

    const std::string style = resolved_style(settings);
    const bool enabled = !arzoom::cursor_style_is_off(style.c_str());
    obs_data_set_bool(settings, SETTING_PRESENTATION_CURSOR, enabled);

    const char *path_value =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
    const std::string custom_path = path_value ? path_value : "";
    const bool custom_auto_key =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY);

    std::string previous_style;
    std::string previous_path;
    bool previous_key = true;
    {
        std::lock_guard<std::mutex> lock(filter->style_mutex);
        previous_style = filter->active_style;
        previous_path = filter->last_custom_path;
        previous_key = filter->last_custom_auto_key;
    }

    phase351_update(filter->phase351, settings);
    set_runtime_enabled(filter, enabled);

    const bool style_changed = style != previous_style;
    const bool custom_source_changed =
        custom_path != previous_path || custom_auto_key != previous_key;

    if (const arzoom::CursorPreset *preset =
            arzoom::find_cursor_preset(style.c_str())) {
        if (style_changed || custom_source_changed)
            load_builtin_preset(filter, *preset);
        Phase35Filter *phase35 = phase35_from_352(filter);
        if (phase35) {
            phase35->hotspot_x.store(preset->hotspot_x,
                                     std::memory_order_release);
            phase35->hotspot_y.store(preset->hotspot_y,
                                     std::memory_order_release);
        }
    } else if (arzoom::cursor_style_is_custom(style.c_str())) {
        if (style_changed)
            load_cursor_asset_v2(filter->phase351,
                                 custom_path, custom_auto_key);
    } else {
        set_runtime_enabled(filter, false);
    }

    {
        std::lock_guard<std::mutex> lock(filter->style_mutex);
        filter->active_style = style;
        filter->last_custom_path = custom_path;
        filter->last_custom_auto_key = custom_auto_key;
    }
}

void phase352_defaults(obs_data_t *settings)
{
    phase35_defaults(settings);
    obs_data_set_default_string(settings,
                                SETTING_PRESENTATION_CURSOR_STYLE,
                                arzoom::kCursorStyleOff);
}

bool cursor_style_modified(obs_properties_t *props, obs_property_t *,
                           obs_data_t *settings)
{
    const char *style =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_STYLE);
    obs_property_t *custom = obs_properties_get(props, CURSOR_CUSTOM_GROUP);
    if (custom)
        obs_property_set_visible(custom,
                                 arzoom::cursor_style_is_custom(style));
    return true;
}

obs_properties_t *phase352_properties(void *data)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    Phase35Filter *phase35 = phase35_from_352(filter);
    obs_properties_t *props = phase3_properties(
        phase35 ? phase35->phase3 : nullptr);

    obs_properties_t *cursor = obs_properties_create();
    obs_property_t *style = obs_properties_add_list(
        cursor, SETTING_PRESENTATION_CURSOR_STYLE,
        obs_module_text("ArZoom.PresentationCursor.Style"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(
        style, obs_module_text("ArZoom.PresentationCursor.Preset.Off"),
        arzoom::kCursorStyleOff);
    for (const auto &preset : arzoom::kCursorPresets)
        obs_property_list_add_string(style,
                                     obs_module_text(preset.locale_key),
                                     preset.id);
    obs_property_list_add_string(
        style, obs_module_text("ArZoom.PresentationCursor.Preset.Custom"),
        arzoom::kCursorStyleCustom);
    obs_property_set_modified_callback(style, cursor_style_modified);

    obs_property_t *size = obs_properties_add_int_slider(
        cursor, SETTING_PRESENTATION_CURSOR_SIZE,
        obs_module_text("ArZoom.PresentationCursor.Size"), 24, 96, 1);
    obs_property_int_set_suffix(size, " px");

    obs_property_t *info = obs_properties_add_text(
        cursor, "presentation_cursor_preset_info",
        obs_module_text("ArZoom.PresentationCursor.PresetInfo"),
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    std::string active_style = arzoom::kCursorStyleOff;
    if (filter) {
        std::lock_guard<std::mutex> lock(filter->style_mutex);
        active_style = filter->active_style;
    }

    obs_properties_t *custom = obs_properties_create();
    obs_properties_add_path(
        custom, SETTING_PRESENTATION_CURSOR_ASSET,
        obs_module_text("ArZoom.PresentationCursor.Asset"), OBS_PATH_FILE,
        "Animated cursor (*.gif *.webp *.png);;All files (*.*)", nullptr);
    obs_property_t *hotspot_x = obs_properties_add_int_slider(
        custom, SETTING_PRESENTATION_CURSOR_HOTSPOT_X,
        obs_module_text("ArZoom.PresentationCursor.HotspotX"), 0, 100, 1);
    obs_property_int_set_suffix(hotspot_x, " %");
    obs_property_t *hotspot_y = obs_properties_add_int_slider(
        custom, SETTING_PRESENTATION_CURSOR_HOTSPOT_Y,
        obs_module_text("ArZoom.PresentationCursor.HotspotY"), 0, 100, 1);
    obs_property_int_set_suffix(hotspot_y, " %");
    obs_properties_add_bool(
        custom, SETTING_PRESENTATION_CURSOR_AUTO_KEY,
        obs_module_text("ArZoom.PresentationCursor.AutoKey"));
    obs_property_t *custom_group = obs_properties_add_group(
        cursor, CURSOR_CUSTOM_GROUP,
        obs_module_text("ArZoom.PresentationCursor.CustomAdvanced"),
        OBS_GROUP_NORMAL, custom);
    obs_property_set_visible(
        custom_group,
        arzoom::cursor_style_is_custom(active_style.c_str()));

    if (filter && !arzoom::cursor_style_is_off(active_style.c_str())) {
        if (native_cursor_probably_enabled(phase1_from_352(filter))) {
            obs_property_t *warning = obs_properties_add_text(
                cursor, "presentation_cursor_native_warning_v2",
                obs_module_text("ArZoom.PresentationCursor.NativeWarning"),
                OBS_TEXT_INFO);
            obs_property_text_set_info_type(warning, OBS_TEXT_INFO_WARNING);
            obs_property_text_set_info_word_wrap(warning, true);
        }
        std::string asset_error;
        if (phase35) {
            std::lock_guard<std::mutex> lock(phase35->asset_mutex);
            asset_error = phase35->asset_error;
        }
        if (!asset_error.empty()) {
            obs_property_t *error = obs_properties_add_text(
                cursor, "presentation_cursor_asset_error_v2",
                asset_error.c_str(), OBS_TEXT_INFO);
            obs_property_text_set_info_type(error, OBS_TEXT_INFO_ERROR);
            obs_property_text_set_info_word_wrap(error, true);
        }
    }

    obs_properties_add_group(
        props, "presentation_cursor_group",
        obs_module_text("ArZoom.PresentationCursor.Group"),
        OBS_GROUP_NORMAL, cursor);
    return props;
}

void phase352_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    if (filter && filter->phase351)
        phase351_tick(filter->phase351, seconds);
}

void phase352_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    if (filter && filter->phase351)
        phase351_render(filter->phase351, effect);
}

void phase352_deactivate(void *data)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    if (filter && filter->phase351)
        phase351_deactivate(filter->phase351);
}

void phase352_destroy(void *data)
{
    auto *filter = static_cast<Phase352Filter *>(data);
    if (!filter)
        return;
    phase351_destroy(filter->phase351);
    delete filter;
}

void *phase352_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase351 = static_cast<Phase351Filter *>(
        phase351_create(settings, context));
    if (!phase351)
        return nullptr;
    auto *filter = new (std::nothrow) Phase352Filter();
    if (!filter) {
        phase351_destroy(phase351);
        return nullptr;
    }
    filter->phase351 = phase351;
    const char *path =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
    filter->last_custom_path = path ? path : "";
    filter->last_custom_auto_key =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY);
    filter->active_style = "__uninitialized__";
    phase352_update(filter, settings);
    blog(LOG_INFO,
         "[ArZoom] Presentation Cursor preset-first runtime ready");
    return filter;
}

struct Phase352SourceInfoOverride {
    Phase352SourceInfoOverride()
    {
        arzoom_filter_info.create = phase352_create;
        arzoom_filter_info.destroy = phase352_destroy;
        arzoom_filter_info.video_tick = phase352_tick;
        arzoom_filter_info.video_render = phase352_render;
        arzoom_filter_info.update = phase352_update;
        arzoom_filter_info.get_properties = phase352_properties;
        arzoom_filter_info.get_defaults = phase352_defaults;
        arzoom_filter_info.deactivate = phase352_deactivate;
    }
};

Phase352SourceInfoOverride phase352_source_info_override;

} // namespace
