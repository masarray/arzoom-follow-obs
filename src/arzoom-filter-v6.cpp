#include "arzoom-filter-v5.cpp"

/*
 * Phase 3.5 trial fix
 * -------------------
 * The first P3.5 trial proved two integration problems that the original unit
 * gates did not cover:
 *
 * 1. QImageReader::read() already advances animated images. Combining read()
 *    with jumpToNextImage() made multi-frame atlas enumeration handler-
 *    dependent and could collapse a GIF to a static cursor at runtime.
 * 2. The first implementation intentionally kept cursor size constant in
 *    output pixels. Direct OBS trial established that presenters expect a
 *    replacement cursor to magnify with the captured screen.
 *
 * This wrapper leaves the frozen P1/P2/P3 camera/control behavior untouched.
 * It replaces only custom-asset preload and applies camera zoom to the cursor
 * display size before delegating to the existing one-pass P3.5 renderer.
 */

namespace {

constexpr float kCursorPlaybackMinSeconds = 0.18f;
constexpr float kCursorPlaybackMaxSeconds = 1.50f;
constexpr float kCursorFallbackFrameSeconds = 0.040f;

struct DecodedCursorAtlasV2 {
    QImage image;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    uint32_t columns = 1;
    uint32_t rows = 1;
    size_t frame_count = 0;
    float play_seconds = kCursorPlaySeconds;
    bool background_keyed = false;
    bool animation_supported = false;
    int reported_image_count = 0;
    std::string error;

    bool valid() const
    {
        return !image.isNull() && frame_width > 0 && frame_height > 0 &&
               frame_count > 0;
    }
};

struct Phase351Filter {
    Phase35Filter *phase35 = nullptr;

    std::atomic<float> base_cursor_size_px{52.0f};
    std::atomic<size_t> pending_frame_count{1};
    std::atomic<float> pending_play_seconds{kCursorPlaySeconds};
    std::atomic<bool> playback_reconfigure{false};

    std::mutex loader_state_mutex;
    std::string loaded_path;
    bool loaded_auto_key = true;
};

DecodedCursorAtlasV2 decode_cursor_atlas_v2(const std::string &path,
                                             bool auto_key)
{
    DecodedCursorAtlasV2 decoded;
    if (path.empty()) {
        decoded.error = "Choose a GIF or image file for Presentation Cursor.";
        return decoded;
    }

    QImageReader reader(QString::fromUtf8(path.c_str()));
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        decoded.error = "Could not read cursor asset: " +
                        reader.errorString().toStdString();
        return decoded;
    }

    decoded.animation_supported = reader.supportsAnimation();
    decoded.reported_image_count = reader.imageCount();

    std::vector<QImage> frames;
    std::vector<float> frame_seconds;
    frames.reserve(32);
    frame_seconds.reserve(32);

    int target_width = 0;
    int target_height = 0;
    bool keyed = false;

    /* Qt's QImageReader contract explicitly says repeated read() calls return
     * consecutive animation frames. Do not pair read() with jumpToNextImage(),
     * which can skip an additional image depending on the format handler. */
    for (size_t index = 0; index < kMaxCursorFrames; ++index) {
        if (index > 0 && !reader.canRead())
            break;

        QImage frame = reader.read();
        if (frame.isNull()) {
            if (frames.empty()) {
                decoded.error = "Cursor asset decoded no usable image frames: " +
                                reader.errorString().toStdString();
                return decoded;
            }
            break;
        }

        if (frames.empty()) {
            int width = frame.width();
            int height = frame.height();
            if (width <= 0 || height <= 0) {
                decoded.error = "Cursor asset has invalid dimensions.";
                return decoded;
            }

            const int max_dimension = std::max(width, height);
            if (max_dimension > kMaxCursorAssetDimension) {
                const float scale =
                    static_cast<float>(kMaxCursorAssetDimension) /
                    static_cast<float>(max_dimension);
                width = std::max(1, static_cast<int>(std::round(width * scale)));
                height = std::max(1, static_cast<int>(std::round(height * scale)));
            }
            target_width = width;
            target_height = height;
        }

        frame = normalize_cursor_frame(
            std::move(frame), target_width, target_height, auto_key, keyed);
        if (frame.isNull()) {
            decoded.error = "Cursor frame normalization failed.";
            return decoded;
        }

        const int delay_ms = reader.nextImageDelay();
        const float delay_seconds = delay_ms > 0
                                        ? static_cast<float>(delay_ms) / 1000.0f
                                        : kCursorFallbackFrameSeconds;
        frames.push_back(std::move(frame));
        frame_seconds.push_back(std::clamp(delay_seconds, 0.010f, 0.250f));
    }

    if (frames.empty()) {
        decoded.error = "Cursor asset contains no frames.";
        return decoded;
    }

    const size_t count = frames.size();
    const uint32_t columns = static_cast<uint32_t>(
        std::ceil(std::sqrt(static_cast<double>(count))));
    const uint32_t rows = static_cast<uint32_t>(
        (count + columns - 1) / columns);

    QImage atlas(target_width * static_cast<int>(columns),
                 target_height * static_cast<int>(rows),
                 QImage::Format_RGBA8888);
    if (atlas.isNull()) {
        decoded.error = "Could not allocate cursor GPU atlas staging image.";
        return decoded;
    }
    atlas.fill(0);

    for (size_t i = 0; i < count; ++i) {
        const int cell_x = static_cast<int>(i % columns) * target_width;
        const int cell_y = static_cast<int>(i / columns) * target_height;
        for (int y = 0; y < target_height; ++y) {
            std::memcpy(atlas.scanLine(cell_y + y) + cell_x * 4,
                        frames[i].constScanLine(y),
                        static_cast<size_t>(target_width) * 4);
        }
    }

    /* Frame 0 is the permanent idle pose, so its baked-in delay does not
     * belong to the click playback. Preserve the asset's remaining cadence as
     * one clean play-once gesture. */
    float play_seconds = 0.0f;
    for (size_t i = 1; i < frame_seconds.size(); ++i)
        play_seconds += frame_seconds[i];
    if (count > 1) {
        if (play_seconds <= 0.0f)
            play_seconds = static_cast<float>(count - 1) *
                           kCursorFallbackFrameSeconds;
        play_seconds = std::clamp(play_seconds,
                                  kCursorPlaybackMinSeconds,
                                  kCursorPlaybackMaxSeconds);
    } else {
        play_seconds = kCursorPlaySeconds;
    }

    decoded.image = std::move(atlas);
    decoded.frame_width = static_cast<uint32_t>(target_width);
    decoded.frame_height = static_cast<uint32_t>(target_height);
    decoded.columns = columns;
    decoded.rows = rows;
    decoded.frame_count = count;
    decoded.play_seconds = play_seconds;
    decoded.background_keyed = keyed;
    return decoded;
}

void queue_playback_configuration(Phase351Filter *filter,
                                  size_t frame_count,
                                  float play_seconds)
{
    filter->pending_frame_count.store(
        std::max<size_t>(1, frame_count), std::memory_order_release);
    filter->pending_play_seconds.store(
        std::clamp(play_seconds, 0.08f, 2.0f), std::memory_order_release);
    filter->playback_reconfigure.store(true, std::memory_order_release);

    /* Suppress the original fixed-duration reconfigure. The wrapper performs
     * the replacement configure on the video tick, where playback is owned. */
    filter->phase35->playback_reconfigure.store(false,
                                                 std::memory_order_release);
}

void clear_cursor_asset_v2(Phase351Filter *filter,
                           const std::string &error,
                           const std::string &path,
                           bool auto_key)
{
    Phase35Filter *phase35 = filter->phase35;
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
        phase35->loaded_path = path;
        phase35->loaded_auto_key = auto_key;
        phase35->asset_error = error;
    }
    destroy_cursor_texture(old_texture);
    phase35->current_frame.store(0, std::memory_order_release);
    queue_playback_configuration(filter, 1, kCursorPlaySeconds);
}

void load_cursor_asset_v2(Phase351Filter *filter,
                          const std::string &path,
                          bool auto_key)
{
    const DecodedCursorAtlasV2 decoded =
        decode_cursor_atlas_v2(path, auto_key);
    if (!decoded.valid()) {
        clear_cursor_asset_v2(filter, decoded.error, path, auto_key);
        if (!decoded.error.empty()) {
            blog(LOG_WARNING, "[ArZoom] Presentation Cursor v2: %s",
                 decoded.error.c_str());
        }
        return;
    }

    const uint8_t *pixels = decoded.image.constBits();
    gs_texture_t *new_texture = nullptr;
    obs_enter_graphics();
    new_texture = gs_texture_create(
        static_cast<uint32_t>(decoded.image.width()),
        static_cast<uint32_t>(decoded.image.height()),
        GS_RGBA, 1, &pixels, 0);
    obs_leave_graphics();

    if (!new_texture) {
        clear_cursor_asset_v2(
            filter, "GPU texture creation failed for cursor asset.",
            path, auto_key);
        blog(LOG_WARNING,
             "[ArZoom] Presentation Cursor v2 GPU atlas creation failed");
        return;
    }

    Phase35Filter *phase35 = filter->phase35;
    gs_texture_t *old_texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(phase35->asset_mutex);
        old_texture = phase35->atlas_texture;
        phase35->atlas_texture = new_texture;
        phase35->frame_width = decoded.frame_width;
        phase35->frame_height = decoded.frame_height;
        phase35->atlas_columns = decoded.columns;
        phase35->atlas_rows = decoded.rows;
        phase35->frame_count = decoded.frame_count;
        phase35->loaded_path = path;
        phase35->loaded_auto_key = auto_key;
        phase35->asset_error.clear();
    }
    destroy_cursor_texture(old_texture);

    phase35->pending_frame_count.store(decoded.frame_count,
                                       std::memory_order_release);
    phase35->current_frame.store(0, std::memory_order_release);
    queue_playback_configuration(filter,
                                 decoded.frame_count,
                                 decoded.play_seconds);

    blog(LOG_INFO,
         "[ArZoom] Presentation Cursor v2 loaded: %zu frame(s), %ux%u, "
         "play %.3fs, animation=%s, reported=%d%s",
         decoded.frame_count,
         decoded.frame_width,
         decoded.frame_height,
         decoded.play_seconds,
         decoded.animation_supported ? "yes" : "no",
         decoded.reported_image_count,
         decoded.background_keyed ? " · solid background removed" : "");

    if (decoded.frame_count <= 1 && decoded.animation_supported) {
        blog(LOG_WARNING,
             "[ArZoom] Animated cursor format was detected but only one frame "
             "was decoded; cursor will remain on the idle frame.");
    }
}

void phase351_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    if (!filter || !filter->phase35)
        return;

    if (filter->playback_reconfigure.exchange(
            false, std::memory_order_acq_rel)) {
        const size_t count =
            filter->pending_frame_count.load(std::memory_order_acquire);
        const float play_seconds =
            filter->pending_play_seconds.load(std::memory_order_acquire);
        filter->phase35->playback.configure(count, play_seconds);
        filter->phase35->current_frame.store(0, std::memory_order_release);
    }

    phase35_tick(filter->phase35, seconds);
}

void phase351_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    if (!filter || !filter->phase35)
        return;

    ArZoomFilter *phase1 = phase1_filter35(filter->phase35);
    const float zoom = phase1 ? std::max(phase1->current_zoom, 1.0f) : 1.0f;
    const float base_size =
        filter->base_cursor_size_px.load(std::memory_order_acquire);

    /* Match the replacement cursor to the magnified captured screen. The
     * existing shader already has a 256 px safety clamp, so normal 52 px at
     * 2x/3x/4x becomes approximately 104/156/208 px without runaway size. */
    filter->phase35->cursor_size_px.store(
        base_size * zoom, std::memory_order_release);
    phase35_render(filter->phase35, effect);
}

void phase351_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    if (!filter || !filter->phase35)
        return;

    phase35_update(filter->phase35, settings);

    const float base_size = std::clamp(
        static_cast<float>(obs_data_get_int(
            settings, SETTING_PRESENTATION_CURSOR_SIZE)),
        24.0f, 96.0f);
    filter->base_cursor_size_px.store(base_size, std::memory_order_release);

    const bool auto_key =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY);
    const char *path_value =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
    const std::string path = path_value ? path_value : "";

    bool reload = false;
    {
        std::lock_guard<std::mutex> lock(filter->loader_state_mutex);
        reload = path != filter->loaded_path ||
                 auto_key != filter->loaded_auto_key;
        if (reload) {
            filter->loaded_path = path;
            filter->loaded_auto_key = auto_key;
        }
    }

    if (reload)
        load_cursor_asset_v2(filter, path, auto_key);
}

void phase351_deactivate(void *data)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    if (filter && filter->phase35)
        phase35_deactivate(filter->phase35);
}

void phase351_destroy(void *data)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    if (!filter)
        return;
    phase35_destroy(filter->phase35);
    delete filter;
}

void *phase351_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase35 = static_cast<Phase35Filter *>(
        phase35_create(settings, context));
    if (!phase35)
        return nullptr;

    auto *filter = new (std::nothrow) Phase351Filter();
    if (!filter) {
        phase35_destroy(phase35);
        return nullptr;
    }
    filter->phase35 = phase35;

    filter->base_cursor_size_px.store(
        std::clamp(static_cast<float>(obs_data_get_int(
                       settings, SETTING_PRESENTATION_CURSOR_SIZE)),
                   24.0f, 96.0f),
        std::memory_order_release);

    const bool auto_key =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY);
    const char *path_value =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
    const std::string path = path_value ? path_value : "";
    {
        std::lock_guard<std::mutex> lock(filter->loader_state_mutex);
        filter->loaded_path = path;
        filter->loaded_auto_key = auto_key;
    }

    /* phase35_create has already performed the original preload once. Replace
     * it immediately with the deterministic reader so the trial never depends
     * on the old read()+jump path. */
    if (!path.empty())
        load_cursor_asset_v2(filter, path, auto_key);
    else
        queue_playback_configuration(filter, 1, kCursorPlaySeconds);

    blog(LOG_INFO,
         "[ArZoom] Presentation Cursor trial fix ready: deterministic GIF "
         "frames + zoom-scaled pointer");
    return filter;
}

struct Phase351SourceInfoOverride {
    Phase351SourceInfoOverride()
    {
        arzoom_filter_info.create = phase351_create;
        arzoom_filter_info.destroy = phase351_destroy;
        arzoom_filter_info.video_tick = phase351_tick;
        arzoom_filter_info.video_render = phase351_render;
        arzoom_filter_info.update = phase351_update;
        arzoom_filter_info.get_properties = phase35_properties;
        arzoom_filter_info.get_defaults = phase35_defaults;
        arzoom_filter_info.deactivate = phase351_deactivate;
    }
};

Phase351SourceInfoOverride phase351_source_info_override;

} // namespace
