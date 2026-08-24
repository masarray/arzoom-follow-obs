#include "arzoom-filter-v9.cpp"

#include <QPainterPath>
#include <QTransform>

/*
 * Phase 3.5 built-in cursor expansion
 * -----------------------------------
 * The uploaded SVG packs are used only as visual references. These additional
 * presets are original ArZoom geometry rendered with QPainter; no third-party
 * SVG data is embedded or redistributed.
 *
 * The three additions intentionally cover distinct presentation moods:
 *   Parakeet Arrow  - vivid modern arrow
 *   Classic Hand    - clean professional hand pointer
 *   Sticker Hand    - friendly expressive hand pointer
 *
 * They use the same short tactile press/rebound curve and one-atlas GPU path as
 * the accepted built-in presets.
 */
namespace {

bool extended_cursor_visual(arzoom::BuiltinCursorVisual visual)
{
    using arzoom::BuiltinCursorVisual;
    return visual == BuiltinCursorVisual::Parakeet ||
           visual == BuiltinCursorVisual::ClassicHand ||
           visual == BuiltinCursorVisual::StickerHand;
}

QPolygonF parakeet_arrow_polygon(float tactile)
{
    static const QPointF points[] = {
        {31.0, 17.0}, {130.0, 72.0}, {91.0, 86.0},
        {118.0, 136.0}, {97.0, 147.0}, {70.0, 98.0}, {43.0, 130.0},
    };
    const QPointF tip(31.0, 17.0);
    const float press = std::max(0.0f, tactile);
    const float rebound = std::max(0.0f, -tactile);
    const float scale = 1.0f - 0.082f * press + 0.060f * rebound;
    const float radians = (-1.45f * press + 0.65f * rebound) *
                          3.14159265f / 180.0f;
    const float ca = std::cos(radians);
    const float sa = std::sin(radians);

    QPolygonF polygon;
    for (const QPointF &point : points) {
        const float dx = static_cast<float>(point.x() - tip.x()) * scale;
        const float dy = static_cast<float>(point.y() - tip.y()) * scale;
        polygon << QPointF(tip.x() + dx * ca - dy * sa,
                           tip.y() + dx * sa + dy * ca);
    }
    return polygon;
}

QPainterPath hand_pointer_path()
{
    QPainterPath path;
    path.moveTo(61.0, 140.0);
    path.cubicTo(51.0, 136.0, 45.0, 128.0, 41.0, 116.0);
    path.lineTo(29.0, 99.0);
    path.cubicTo(24.5, 92.5, 26.0, 85.0, 32.0, 81.0);
    path.cubicTo(37.0, 77.5, 43.0, 79.0, 48.0, 84.0);
    path.lineTo(54.0, 90.0);
    path.lineTo(54.0, 24.0);
    path.cubicTo(54.0, 15.0, 59.5, 9.0, 67.0, 9.0);
    path.cubicTo(74.5, 9.0, 80.0, 15.0, 80.0, 24.0);
    path.lineTo(80.0, 68.0);
    path.lineTo(82.0, 49.0);
    path.cubicTo(82.0, 41.0, 87.0, 35.5, 94.0, 35.5);
    path.cubicTo(101.0, 35.5, 106.0, 41.0, 106.0, 49.0);
    path.lineTo(106.0, 70.0);
    path.lineTo(108.0, 55.0);
    path.cubicTo(108.0, 47.0, 113.0, 42.0, 120.0, 42.0);
    path.cubicTo(127.0, 42.0, 132.0, 47.0, 132.0, 55.0);
    path.lineTo(132.0, 75.0);
    path.lineTo(134.0, 64.0);
    path.cubicTo(134.0, 57.0, 138.5, 52.5, 145.0, 52.5);
    path.cubicTo(151.0, 52.5, 154.0, 58.0, 153.0, 65.0);
    path.lineTo(151.5, 97.0);
    path.cubicTo(151.0, 116.0, 140.0, 132.0, 124.0, 140.0);
    path.cubicTo(108.0, 148.0, 77.0, 147.0, 61.0, 140.0);
    path.closeSubpath();
    return path;
}

QTransform hand_tactile_transform(float tactile, const QPointF &hotspot)
{
    const float press = std::max(0.0f, tactile);
    const float rebound = std::max(0.0f, -tactile);
    const float sx = 1.0f - 0.055f * press + 0.045f * rebound;
    const float sy = 1.0f - 0.075f * press + 0.055f * rebound;
    const float rotation = 0.85f * press - 0.35f * rebound;

    QTransform transform;
    transform.translate(hotspot.x(), hotspot.y());
    transform.rotate(rotation);
    transform.scale(sx, sy);
    transform.translate(-hotspot.x(), -hotspot.y());
    return transform;
}

void draw_parakeet_arrow(QPainter &painter, float tactile)
{
    const QPolygonF polygon = parakeet_arrow_polygon(tactile);
    const float press = std::max(0.0f, tactile);

    QPolygonF shadow = polygon;
    for (QPointF &point : shadow)
        point += QPointF(-3.0, 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(27, 52, 80, 205));
    painter.drawPolygon(shadow);

    QLinearGradient fill(34, 18, 112, 143);
    fill.setColorAt(0.0, QColor(83, 235, 187));
    fill.setColorAt(0.48, QColor(29, 218, 218));
    fill.setColorAt(1.0, QColor(24, 174, 225));
    painter.setBrush(fill);
    painter.setPen(QPen(QColor(35, 57, 83), 3.2,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolygon(polygon);

    painter.setPen(QPen(QColor(205, 255, 232,
                              static_cast<int>(190.0f - 45.0f * press)),
                        2.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(polygon[0] * 0.86 + polygon[1] * 0.14,
                     polygon[0] * 0.40 + polygon[1] * 0.60);
}

void draw_classic_hand(QPainter &painter, float tactile)
{
    const QPointF hotspot(67.0, 9.0);
    const QTransform tactile_transform = hand_tactile_transform(tactile, hotspot);
    const QPainterPath path = tactile_transform.map(hand_pointer_path());
    const float press = std::max(0.0f, tactile);

    QPainterPath shadow = QTransform::fromTranslate(3.0, 5.0).map(path);
    painter.setBrush(QColor(12, 22, 36, 115));
    painter.setPen(Qt::NoPen);
    painter.drawPath(shadow);

    painter.setBrush(QColor(248, 250, 253));
    painter.setPen(QPen(QColor(24, 31, 42), 5.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);

    const QRectF cuff = tactile_transform.mapRect(QRectF(61.0, 128.0, 64.0, 18.0));
    painter.setBrush(QColor(113, 159, 205));
    painter.setPen(QPen(QColor(24, 31, 42), 4.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(cuff, 5.0, 5.0);

    if (press > 0.10f) {
        painter.setPen(QPen(QColor(72, 126, 180,
                                  static_cast<int>(150.0f * press)),
                            2.4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(tactile_transform.map(QPointF(58.0, 31.0)),
                         tactile_transform.map(QPointF(75.0, 31.0)));
    }
}

void draw_sticker_hand(QPainter &painter, float tactile)
{
    const QPointF hotspot(67.0, 9.0);
    const QTransform tactile_transform = hand_tactile_transform(tactile, hotspot);
    const QPainterPath path = tactile_transform.map(hand_pointer_path());
    const float press = std::max(0.0f, tactile);

    QPainterPath shadow = QTransform::fromTranslate(4.0, 6.0).map(path);
    painter.setBrush(QColor(72, 58, 103, 105));
    painter.setPen(QPen(QColor(72, 58, 103, 65), 10.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(shadow);

    painter.setBrush(QColor(255, 169, 112));
    painter.setPen(QPen(QColor(255, 252, 245), 12.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    painter.setPen(QPen(QColor(78, 65, 113), 4.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);

    const QRectF cuff = tactile_transform.mapRect(QRectF(60.0, 127.0, 66.0, 20.0));
    painter.setBrush(QColor(98, 191, 244));
    painter.setPen(QPen(QColor(78, 65, 113), 4.0,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(cuff, 6.0, 6.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(64, 218, 202,
                            static_cast<int>(90.0f + 110.0f * press)));
    const QPointF accent = tactile_transform.map(QPointF(119.0, 100.0));
    painter.drawEllipse(accent, 4.0 + 1.5 * press, 4.0 + 1.5 * press);
}

QImage build_extended_cursor_atlas(const arzoom::CursorPreset &preset)
{
    if (!extended_cursor_visual(preset.visual))
        return {};

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
        QPainter painter(&frame);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const float tactile = arzoom::cursor_tactile_press(i, preset.frame_count);
        switch (preset.visual) {
        case arzoom::BuiltinCursorVisual::Parakeet:
            draw_parakeet_arrow(painter, tactile);
            break;
        case arzoom::BuiltinCursorVisual::ClassicHand:
            draw_classic_hand(painter, tactile);
            break;
        case arzoom::BuiltinCursorVisual::StickerHand:
            draw_sticker_hand(painter, tactile);
            break;
        default:
            painter.end();
            atlas_painter.end();
            return {};
        }
        painter.end();

        const int x = static_cast<int>(i % kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        const int y = static_cast<int>(i / kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        atlas_painter.drawImage(x, y, frame);
    }
    atlas_painter.end();
    return atlas;
}

bool load_extended_cursor_preset(Phase352Filter *filter,
                                 const arzoom::CursorPreset &preset)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    if (!phase35 || !arzoom::cursor_preset_is_valid(preset) ||
        !extended_cursor_visual(preset.visual))
        return false;

    QImage atlas = build_extended_cursor_atlas(preset);
    if (atlas.isNull()) {
        clear_builtin_texture(filter,
                              "Could not allocate extended cursor atlas.");
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
                              "GPU texture creation failed for extended cursor.");
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
         "[ArZoom] Extended Presentation Cursor ready: %s (%zu frames, %.0f ms)",
         preset.id, preset.frame_count, preset.play_seconds * 1000.0f);
    return true;
}

struct Phase354Filter {
    Phase353Filter *phase353 = nullptr;
};

void phase354_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    if (!filter || !filter->phase353)
        return;

    phase353_update(filter->phase353, settings);

    const std::string style = resolved_style(settings);
    if (const arzoom::CursorPreset *preset =
            arzoom::find_cursor_preset(style.c_str())) {
        if (extended_cursor_visual(preset->visual))
            load_extended_cursor_preset(filter->phase353->phase352, *preset);
    }
}

obs_properties_t *phase354_properties(void *data)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    return phase353_properties(filter ? filter->phase353 : nullptr);
}

void phase354_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    if (filter && filter->phase353)
        phase353_tick(filter->phase353, seconds);
}

void phase354_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    if (filter && filter->phase353)
        phase353_render(filter->phase353, effect);
}

void phase354_deactivate(void *data)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    if (filter && filter->phase353)
        phase353_deactivate(filter->phase353);
}

void phase354_destroy(void *data)
{
    auto *filter = static_cast<Phase354Filter *>(data);
    if (!filter)
        return;
    phase353_destroy(filter->phase353);
    delete filter;
}

void *phase354_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase353 = static_cast<Phase353Filter *>(
        phase353_create(settings, context));
    if (!phase353)
        return nullptr;

    auto *filter = new (std::nothrow) Phase354Filter();
    if (!filter) {
        phase353_destroy(phase353);
        return nullptr;
    }
    filter->phase353 = phase353;
    phase354_update(filter, settings);

    blog(LOG_INFO,
         "[ArZoom] Extended built-in cursor palette ready");
    return filter;
}

struct Phase354SourceInfoOverride {
    Phase354SourceInfoOverride()
    {
        arzoom_filter_info.create = phase354_create;
        arzoom_filter_info.destroy = phase354_destroy;
        arzoom_filter_info.video_tick = phase354_tick;
        arzoom_filter_info.video_render = phase354_render;
        arzoom_filter_info.update = phase354_update;
        arzoom_filter_info.get_properties = phase354_properties;
        arzoom_filter_info.get_defaults = phase352_defaults;
        arzoom_filter_info.deactivate = phase354_deactivate;
    }
};

Phase354SourceInfoOverride phase354_source_info_override;

} // namespace
