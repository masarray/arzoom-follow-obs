#include "arzoom-filter-v8.cpp"

/*
 * Phase 3.5 tactile polish
 * ------------------------
 * Direct OBS trial accepted the built-in cursor artwork but found the original
 * 1.1s animation too slow and decorative. Keep the accepted visual designs,
 * then retime their generated frames into a short physical micro-interaction:
 *
 *   fast press -> elastic release -> tiny rebound -> exact settle
 *
 * This remains preset-load-time work only. The video hot path still advances
 * one integer frame index and performs no image decoding or allocation.
 */
namespace {

constexpr size_t kLegacyPressPeakFrame = 10;

QImage builtin_atlas_frame(const QImage &atlas, size_t frame_index)
{
    if (atlas.isNull())
        return {};
    const int x = static_cast<int>(frame_index % kBuiltinCursorColumns) *
                  kBuiltinCursorFrameSize;
    const int y = static_cast<int>(frame_index / kBuiltinCursorColumns) *
                  kBuiltinCursorFrameSize;
    return atlas.copy(x, y, kBuiltinCursorFrameSize,
                      kBuiltinCursorFrameSize);
}

QImage rebound_frame_from_idle(const QImage &idle,
                               const arzoom::CursorPreset &preset,
                               float tactile_press)
{
    if (idle.isNull())
        return {};

    QImage frame(kBuiltinCursorFrameSize, kBuiltinCursorFrameSize,
                 QImage::Format_RGBA8888);
    frame.fill(Qt::transparent);

    const float rebound = std::max(0.0f, -tactile_press);
    const float scale = 1.0f + rebound * 0.10f;
    const QPointF hotspot(
        preset.hotspot_x * kBuiltinCursorFrameSize,
        preset.hotspot_y * kBuiltinCursorFrameSize);

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(hotspot);
    painter.scale(scale, scale);
    painter.translate(-hotspot);
    painter.drawImage(0, 0, idle);
    painter.end();
    return frame;
}

QImage build_tactile_builtin_atlas(const arzoom::CursorPreset &preset)
{
    const QImage visual_source = build_builtin_atlas(preset);
    if (visual_source.isNull())
        return {};

    const QImage idle = builtin_atlas_frame(visual_source, 0);
    if (idle.isNull())
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
        const float tactile = arzoom::cursor_tactile_press(
            i, preset.frame_count);
        QImage frame;

        if (i == 0) {
            frame = idle;
        } else if (tactile >= 0.0f) {
            /* Reuse the accepted visual artwork's rising press frames. This
             * preserves its fill/highlight treatment while compressing the
             * old slow gesture into the tactile curve's fast press phase. */
            const float amount = std::clamp(tactile, 0.0f, 1.0f);
            const size_t source_frame = std::clamp<size_t>(
                1 + static_cast<size_t>(std::lround(
                        amount * static_cast<float>(kLegacyPressPeakFrame - 1))),
                1, kLegacyPressPeakFrame);
            frame = builtin_atlas_frame(visual_source, source_frame);
        } else {
            /* Negative tactile values are the tiny overshoot beyond neutral.
             * Scale around the click hotspot so the tip never drifts. */
            frame = rebound_frame_from_idle(idle, preset, tactile);
        }

        if (frame.isNull()) {
            atlas_painter.end();
            return {};
        }

        const int x = static_cast<int>(i % kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        const int y = static_cast<int>(i / kBuiltinCursorColumns) *
                      kBuiltinCursorFrameSize;
        atlas_painter.drawImage(x, y, frame);
    }

    atlas_painter.end();
    return atlas;
}

bool load_tactile_builtin_preset(Phase352Filter *filter,
                                 const arzoom::CursorPreset &preset)
{
    Phase35Filter *phase35 = phase35_from_352(filter);
    if (!phase35 || !arzoom::cursor_preset_is_valid(preset))
        return false;

    QImage atlas = build_tactile_builtin_atlas(preset);
    if (atlas.isNull()) {
        clear_builtin_texture(filter,
                              "Could not allocate tactile cursor atlas.");
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
                              "GPU texture creation failed for tactile cursor.");
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
         "[ArZoom] Tactile Presentation Cursor ready: %s (%zu frames, %.0f ms)",
         preset.id, preset.frame_count, preset.play_seconds * 1000.0f);
    return true;
}

struct Phase353Filter {
    Phase352Filter *phase352 = nullptr;
};

void phase353_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    if (!filter || !filter->phase352)
        return;

    phase352_update(filter->phase352, settings);

    const std::string style = resolved_style(settings);
    if (const arzoom::CursorPreset *preset =
            arzoom::find_cursor_preset(style.c_str())) {
        /* Settings updates are rare. Rebuilding here keeps the wrapper simple
         * and guarantees the old non-tactile atlas can never win after a size,
         * style, or unrelated property update. */
        load_tactile_builtin_preset(filter->phase352, *preset);
    }
}

obs_properties_t *phase353_properties(void *data)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    return phase352_properties(filter ? filter->phase352 : nullptr);
}

void phase353_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    if (filter && filter->phase352)
        phase352_tick(filter->phase352, seconds);
}

void phase353_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    if (filter && filter->phase352)
        phase352_render(filter->phase352, effect);
}

void phase353_deactivate(void *data)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    if (filter && filter->phase352)
        phase352_deactivate(filter->phase352);
}

void phase353_destroy(void *data)
{
    auto *filter = static_cast<Phase353Filter *>(data);
    if (!filter)
        return;
    phase352_destroy(filter->phase352);
    delete filter;
}

void *phase353_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase352 = static_cast<Phase352Filter *>(
        phase352_create(settings, context));
    if (!phase352)
        return nullptr;

    auto *filter = new (std::nothrow) Phase353Filter();
    if (!filter) {
        phase352_destroy(phase352);
        return nullptr;
    }
    filter->phase352 = phase352;
    phase353_update(filter, settings);

    blog(LOG_INFO,
         "[ArZoom] Tactile cursor micro-interaction runtime ready");
    return filter;
}

struct Phase353SourceInfoOverride {
    Phase353SourceInfoOverride()
    {
        arzoom_filter_info.create = phase353_create;
        arzoom_filter_info.destroy = phase353_destroy;
        arzoom_filter_info.video_tick = phase353_tick;
        arzoom_filter_info.video_render = phase353_render;
        arzoom_filter_info.update = phase353_update;
        arzoom_filter_info.get_properties = phase353_properties;
        arzoom_filter_info.get_defaults = phase352_defaults;
        arzoom_filter_info.deactivate = phase353_deactivate;
    }
};

Phase353SourceInfoOverride phase353_source_info_override;

} // namespace
