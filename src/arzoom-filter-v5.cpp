#include "arzoom-presentation-cursor.hpp"

#include <QImage>
#include <QImageReader>
#include <QString>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

/*
 * Phase 3.5 layers an optional animated Presentation Cursor around the frozen
 * v0.4.0 Presenter Controls runtime. The cursor is decoded only when its asset
 * changes, packed into one GPU atlas, and then rendered in the existing
 * presentation pass. No GIF/image decoding occurs on click or per frame.
 */
#include "arzoom-filter-v4.cpp"

#define SETTING_PRESENTATION_CURSOR "presentation_cursor_enabled"
#define SETTING_PRESENTATION_CURSOR_ASSET "presentation_cursor_asset"
#define SETTING_PRESENTATION_CURSOR_SIZE "presentation_cursor_size"
#define SETTING_PRESENTATION_CURSOR_HOTSPOT_X "presentation_cursor_hotspot_x"
#define SETTING_PRESENTATION_CURSOR_HOTSPOT_Y "presentation_cursor_hotspot_y"
#define SETTING_PRESENTATION_CURSOR_AUTO_KEY "presentation_cursor_auto_key"

namespace {

constexpr size_t kMaxCursorFrames = 64;
constexpr int kMaxCursorAssetDimension = 512;
constexpr float kCursorPlaySeconds = 0.46f;

struct DecodedCursorAtlas {
    QImage image;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    uint32_t columns = 1;
    uint32_t rows = 1;
    size_t frame_count = 0;
    bool background_keyed = false;
    std::string error;

    bool valid() const
    {
        return !image.isNull() && frame_width > 0 && frame_height > 0 &&
               frame_count > 0;
    }
};

struct Phase35Filter {
    Phase3Filter *phase3 = nullptr;

    std::atomic<bool> cursor_enabled{false};
    std::atomic<float> cursor_size_px{52.0f};
    std::atomic<float> hotspot_x{0.18f};
    std::atomic<float> hotspot_y{0.18f};

    /* Video-tick-owned playback; only the selected frame crosses to render. */
    arzoom::PresentationCursorPlayback playback;
    std::atomic<size_t> pending_frame_count{1};
    std::atomic<bool> playback_reconfigure{false};
    std::atomic<int> current_frame{0};
    uint32_t last_click_generation = 0;

    std::atomic<float> cursor_content_x{0.5f};
    std::atomic<float> cursor_content_y{0.5f};
    std::atomic<bool> cursor_position_valid{false};

    /* Resource mutation is rare (asset/settings changes). Rendering takes this
     * mutex only when a custom cursor is actually visible. */
    std::mutex asset_mutex;
    gs_texture_t *atlas_texture = nullptr;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    uint32_t atlas_columns = 1;
    uint32_t atlas_rows = 1;
    size_t frame_count = 0;
    std::string loaded_path;
    bool loaded_auto_key = true;
    std::string asset_error;

    gs_eparam_t *cursor_atlas_param = nullptr;
    gs_eparam_t *cursor_content_param = nullptr;
    gs_eparam_t *cursor_asset_size_param = nullptr;
    gs_eparam_t *cursor_hotspot_param = nullptr;
    gs_eparam_t *cursor_atlas_grid_param = nullptr;
    gs_eparam_t *cursor_frame_param = nullptr;
    gs_eparam_t *cursor_size_param = nullptr;
    gs_eparam_t *cursor_visible_param = nullptr;
    bool cursor_shader_ready = false;
};

Phase2Filter *phase2_filter(Phase35Filter *filter)
{
    return filter && filter->phase3 ? filter->phase3->phase2 : nullptr;
}

ArZoomFilter *phase1_filter35(Phase35Filter *filter)
{
    Phase2Filter *phase2 = phase2_filter(filter);
    return phase2 ? phase2->phase1 : nullptr;
}

int color_distance_squared(const uint8_t *a, const uint8_t *b)
{
    const int dr = static_cast<int>(a[0]) - static_cast<int>(b[0]);
    const int dg = static_cast<int>(a[1]) - static_cast<int>(b[1]);
    const int db = static_cast<int>(a[2]) - static_cast<int>(b[2]);
    return dr * dr + dg * dg + db * db;
}

bool auto_key_solid_background(QImage &image)
{
    if (image.isNull() || image.width() < 2 || image.height() < 2)
        return false;

    image = image.convertToFormat(QImage::Format_RGBA8888);
    const int width = image.width();
    const int height = image.height();

    const uint8_t *corners[4] = {
        image.constScanLine(0),
        image.constScanLine(0) + (width - 1) * 4,
        image.constScanLine(height - 1),
        image.constScanLine(height - 1) + (width - 1) * 4,
    };

    for (const uint8_t *corner : corners) {
        if (corner[3] < 250)
            return false; /* Asset already carries transparency. */
    }

    constexpr int kCornerAgreementSquared = 18 * 18 * 3;
    for (int i = 1; i < 4; ++i) {
        if (color_distance_squared(corners[0], corners[i]) >
            kCornerAgreementSquared) {
            return false;
        }
    }

    const int bg_r = (static_cast<int>(corners[0][0]) + corners[1][0] +
                      corners[2][0] + corners[3][0]) / 4;
    const int bg_g = (static_cast<int>(corners[0][1]) + corners[1][1] +
                      corners[2][1] + corners[3][1]) / 4;
    const int bg_b = (static_cast<int>(corners[0][2]) + corners[1][2] +
                      corners[2][2] + corners[3][2]) / 4;

    constexpr float kTransparentDistance = 8.0f;
    constexpr float kOpaqueDistance = 42.0f;
    bool changed = false;

    for (int y = 0; y < height; ++y) {
        uint8_t *row = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            uint8_t *pixel = row + x * 4;
            const float dr = static_cast<float>(pixel[0]) - bg_r;
            const float dg = static_cast<float>(pixel[1]) - bg_g;
            const float db = static_cast<float>(pixel[2]) - bg_b;
            const float distance = std::sqrt(dr * dr + dg * dg + db * db);
            if (distance >= kOpaqueDistance)
                continue;

            float keep = (distance - kTransparentDistance) /
                         (kOpaqueDistance - kTransparentDistance);
            keep = std::clamp(keep, 0.0f, 1.0f);
            const uint8_t next_alpha = static_cast<uint8_t>(
                static_cast<float>(pixel[3]) * keep + 0.5f);
            changed = changed || next_alpha != pixel[3];
            pixel[3] = next_alpha;
        }
    }
    return changed;
}

QImage normalize_cursor_frame(QImage frame, int target_width,
                              int target_height, bool auto_key,
                              bool &background_keyed)
{
    if (frame.isNull())
        return {};

    frame = frame.convertToFormat(QImage::Format_RGBA8888);
    if (target_width > 0 && target_height > 0 &&
        (frame.width() != target_width || frame.height() != target_height)) {
        frame = frame.scaled(target_width, target_height,
                             Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    if (auto_key)
        background_keyed =
            auto_key_solid_background(frame) || background_keyed;
    return frame;
}

DecodedCursorAtlas decode_cursor_atlas(const std::string &path,
                                       bool auto_key)
{
    DecodedCursorAtlas decoded;
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

    std::vector<QImage> frames;
    frames.reserve(32);

    int target_width = 0;
    int target_height = 0;
    bool keyed = false;
    for (size_t index = 0; index < kMaxCursorFrames; ++index) {
        QImage frame = reader.read();
        if (frame.isNull()) {
            if (frames.empty()) {
                decoded.error = "Cursor asset decoded no usable image frames.";
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
        frames.push_back(std::move(frame));

        if (!reader.supportsAnimation() || !reader.jumpToNextImage())
            break;
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

    decoded.image = std::move(atlas);
    decoded.frame_width = static_cast<uint32_t>(target_width);
    decoded.frame_height = static_cast<uint32_t>(target_height);
    decoded.columns = columns;
    decoded.rows = rows;
    decoded.frame_count = count;
    decoded.background_keyed = keyed;
    return decoded;
}

void destroy_cursor_texture(gs_texture_t *texture)
{
    if (!texture)
        return;
    obs_enter_graphics();
    gs_texture_destroy(texture);
    obs_leave_graphics();
}

void clear_cursor_asset(Phase35Filter *filter, const std::string &error,
                        const std::string &path, bool auto_key)
{
    gs_texture_t *old_texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(filter->asset_mutex);
        old_texture = filter->atlas_texture;
        filter->atlas_texture = nullptr;
        filter->frame_width = 0;
        filter->frame_height = 0;
        filter->atlas_columns = 1;
        filter->atlas_rows = 1;
        filter->frame_count = 0;
        filter->loaded_path = path;
        filter->loaded_auto_key = auto_key;
        filter->asset_error = error;
    }
    destroy_cursor_texture(old_texture);
    filter->pending_frame_count.store(1, std::memory_order_release);
    filter->playback_reconfigure.store(true, std::memory_order_release);
    filter->current_frame.store(0, std::memory_order_release);
}

void load_cursor_asset(Phase35Filter *filter, const std::string &path,
                       bool auto_key)
{
    const DecodedCursorAtlas decoded = decode_cursor_atlas(path, auto_key);
    if (!decoded.valid()) {
        clear_cursor_asset(filter, decoded.error, path, auto_key);
        if (!decoded.error.empty())
            blog(LOG_WARNING, "[ArZoom] Presentation Cursor: %s",
                 decoded.error.c_str());
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
        clear_cursor_asset(filter,
                           "GPU texture creation failed for cursor asset.",
                           path, auto_key);
        blog(LOG_WARNING,
             "[ArZoom] Presentation Cursor GPU atlas creation failed");
        return;
    }

    gs_texture_t *old_texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(filter->asset_mutex);
        old_texture = filter->atlas_texture;
        filter->atlas_texture = new_texture;
        filter->frame_width = decoded.frame_width;
        filter->frame_height = decoded.frame_height;
        filter->atlas_columns = decoded.columns;
        filter->atlas_rows = decoded.rows;
        filter->frame_count = decoded.frame_count;
        filter->loaded_path = path;
        filter->loaded_auto_key = auto_key;
        filter->asset_error.clear();
    }
    destroy_cursor_texture(old_texture);

    filter->pending_frame_count.store(decoded.frame_count,
                                      std::memory_order_release);
    filter->playback_reconfigure.store(true, std::memory_order_release);
    filter->current_frame.store(0, std::memory_order_release);

    blog(LOG_INFO,
         "[ArZoom] Presentation Cursor loaded: %zu frame(s), %ux%u%s",
         decoded.frame_count, decoded.frame_width, decoded.frame_height,
         decoded.background_keyed ? " · solid background removed" : "");
}

bool target_captures_native_cursor(ArZoomFilter *phase1)
{
    if (!phase1)
        return false;
    obs_source_t *target = obs_filter_get_target(phase1->context);
    if (!target)
        return false;
    obs_data_t *settings = obs_source_get_settings(target);
    if (!settings)
        return false;
    const bool has_value = obs_data_has_user_value(settings, "capture_cursor");
    const bool captures = has_value &&
                          obs_data_get_bool(settings, "capture_cursor");
    obs_data_release(settings);
    return captures;
}

uint32_t newest_click_generation(Phase2Filter *phase2)
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

void update_live_cursor_position(Phase35Filter *filter)
{
    ArZoomFilter *phase1 = phase1_filter35(filter);
    arzoom::Vec2 cursor;
    const bool valid = normalized_cursor_for_filter(phase1, cursor);
    if (valid) {
        filter->cursor_content_x.store(cursor.x, std::memory_order_release);
        filter->cursor_content_y.store(cursor.y, std::memory_order_release);
    }
    filter->cursor_position_valid.store(valid, std::memory_order_release);
}

void phase35_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    if (!filter || !filter->phase3)
        return;

    const float dt = std::clamp(seconds, 0.0f, 0.10f);
    const bool enabled = filter->cursor_enabled.load(std::memory_order_acquire);
    Phase2Filter *phase2 = phase2_filter(filter);
    if (phase2) {
        phase2->click_capture_for_cursor.store(
            enabled, std::memory_order_release);
    }

    /* Frozen P3 camera/control behavior executes first and owns camera state. */
    phase3_tick(filter->phase3, seconds);

    if (filter->playback_reconfigure.exchange(
            false, std::memory_order_acq_rel)) {
        filter->playback.configure(
            filter->pending_frame_count.load(std::memory_order_acquire),
            kCursorPlaySeconds);
    }

    if (!enabled) {
        filter->playback.reset();
        filter->current_frame.store(0, std::memory_order_release);
        filter->cursor_position_valid.store(false, std::memory_order_release);
        return;
    }

    update_live_cursor_position(filter);

    const uint32_t newest = newest_click_generation(phase2);
    if (newest > filter->last_click_generation) {
        filter->last_click_generation = newest;
        filter->playback.trigger();
    }
    filter->playback.advance(dt);
    filter->current_frame.store(
        static_cast<int>(filter->playback.frame_index()),
        std::memory_order_release);
}

void phase35_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    if (!filter)
        return;

    phase3_update(filter->phase3, settings);

    const bool enabled =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR);
    const float size_px = std::clamp(
        static_cast<float>(obs_data_get_int(
            settings, SETTING_PRESENTATION_CURSOR_SIZE)),
        24.0f, 96.0f);
    const float hotspot_x = std::clamp(
        static_cast<float>(obs_data_get_int(
            settings, SETTING_PRESENTATION_CURSOR_HOTSPOT_X)) / 100.0f,
        0.0f, 1.0f);
    const float hotspot_y = std::clamp(
        static_cast<float>(obs_data_get_int(
            settings, SETTING_PRESENTATION_CURSOR_HOTSPOT_Y)) / 100.0f,
        0.0f, 1.0f);
    const bool auto_key =
        obs_data_get_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY);
    const char *path_value =
        obs_data_get_string(settings, SETTING_PRESENTATION_CURSOR_ASSET);
    const std::string path = path_value ? path_value : "";

    filter->cursor_enabled.store(enabled, std::memory_order_release);
    filter->cursor_size_px.store(size_px, std::memory_order_release);
    filter->hotspot_x.store(hotspot_x, std::memory_order_release);
    filter->hotspot_y.store(hotspot_y, std::memory_order_release);

    if (Phase2Filter *phase2 = phase2_filter(filter)) {
        phase2->click_capture_for_cursor.store(
            enabled, std::memory_order_release);
    }

    bool reload = false;
    {
        std::lock_guard<std::mutex> lock(filter->asset_mutex);
        reload = path != filter->loaded_path ||
                 auto_key != filter->loaded_auto_key;
    }
    if (reload)
        load_cursor_asset(filter, path, auto_key);

    if (!enabled) {
        filter->cursor_position_valid.store(false, std::memory_order_release);
        filter->current_frame.store(0, std::memory_order_release);
    }
}

void phase35_defaults(obs_data_t *settings)
{
    phase3_defaults(settings);
    obs_data_set_default_bool(settings, SETTING_PRESENTATION_CURSOR, false);
    obs_data_set_default_string(settings, SETTING_PRESENTATION_CURSOR_ASSET, "");
    obs_data_set_default_int(settings, SETTING_PRESENTATION_CURSOR_SIZE, 52);
    obs_data_set_default_int(settings, SETTING_PRESENTATION_CURSOR_HOTSPOT_X, 18);
    obs_data_set_default_int(settings, SETTING_PRESENTATION_CURSOR_HOTSPOT_Y, 18);
    obs_data_set_default_bool(settings, SETTING_PRESENTATION_CURSOR_AUTO_KEY, true);
}

obs_properties_t *phase35_properties(void *data)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    obs_properties_t *props = phase3_properties(
        filter ? filter->phase3 : nullptr);

    obs_properties_t *cursor = obs_properties_create();
    obs_properties_add_bool(
        cursor, SETTING_PRESENTATION_CURSOR,
        obs_module_text("ArZoom.PresentationCursor.Enabled"));
    obs_properties_add_path(
        cursor, SETTING_PRESENTATION_CURSOR_ASSET,
        obs_module_text("ArZoom.PresentationCursor.Asset"),
        OBS_PATH_FILE,
        "Animated cursor (*.gif *.webp *.png);;All files (*.*)",
        nullptr);
    obs_property_t *size = obs_properties_add_int_slider(
        cursor, SETTING_PRESENTATION_CURSOR_SIZE,
        obs_module_text("ArZoom.PresentationCursor.Size"),
        24, 96, 1);
    obs_property_int_set_suffix(size, " px");

    obs_property_t *info = obs_properties_add_text(
        cursor, "presentation_cursor_info",
        obs_module_text("ArZoom.PresentationCursor.Info"), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_property_text_set_info_word_wrap(info, true);

    if (filter) {
        ArZoomFilter *phase1 = phase1_filter35(filter);
        const bool enabled =
            filter->cursor_enabled.load(std::memory_order_acquire);
        if (enabled && target_captures_native_cursor(phase1)) {
            obs_property_t *warning = obs_properties_add_text(
                cursor, "presentation_cursor_native_warning",
                obs_module_text("ArZoom.PresentationCursor.NativeWarning"),
                OBS_TEXT_INFO);
            obs_property_text_set_info_type(warning, OBS_TEXT_INFO_WARNING);
            obs_property_text_set_info_word_wrap(warning, true);
        }

        std::string asset_error;
        {
            std::lock_guard<std::mutex> lock(filter->asset_mutex);
            asset_error = filter->asset_error;
        }
        if (enabled && !asset_error.empty()) {
            obs_property_t *error = obs_properties_add_text(
                cursor, "presentation_cursor_asset_error",
                asset_error.c_str(), OBS_TEXT_INFO);
            obs_property_text_set_info_type(error, OBS_TEXT_INFO_ERROR);
            obs_property_text_set_info_word_wrap(error, true);
        }
    }

    obs_properties_t *advanced = obs_properties_create();
    obs_property_t *hotspot_x = obs_properties_add_int_slider(
        advanced, SETTING_PRESENTATION_CURSOR_HOTSPOT_X,
        obs_module_text("ArZoom.PresentationCursor.HotspotX"),
        0, 100, 1);
    obs_property_int_set_suffix(hotspot_x, " %");
    obs_property_t *hotspot_y = obs_properties_add_int_slider(
        advanced, SETTING_PRESENTATION_CURSOR_HOTSPOT_Y,
        obs_module_text("ArZoom.PresentationCursor.HotspotY"),
        0, 100, 1);
    obs_property_int_set_suffix(hotspot_y, " %");
    obs_properties_add_bool(
        advanced, SETTING_PRESENTATION_CURSOR_AUTO_KEY,
        obs_module_text("ArZoom.PresentationCursor.AutoKey"));
    obs_properties_add_group(
        cursor, "presentation_cursor_advanced",
        obs_module_text("ArZoom.PresentationCursor.Advanced"),
        OBS_GROUP_NORMAL, advanced);

    obs_properties_add_group(
        props, "presentation_cursor_group",
        obs_module_text("ArZoom.PresentationCursor.Group"),
        OBS_GROUP_NORMAL, cursor);
    return props;
}

void set_cursor_hidden(Phase35Filter *filter)
{
    if (filter && filter->cursor_visible_param)
        gs_effect_set_float(filter->cursor_visible_param, 0.0f);
}

void phase35_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    if (!filter || !filter->phase3)
        return;

    ArZoomFilter *phase1 = phase1_filter35(filter);
    Phase2Filter *phase2 = phase2_filter(filter);
    const bool cursor_active =
        filter->cursor_enabled.load(std::memory_order_acquire) &&
        filter->cursor_position_valid.load(std::memory_order_acquire) &&
        filter->cursor_shader_ready;

    if (!cursor_active) {
        set_cursor_hidden(filter);
        phase3_render(filter->phase3, effect);
        return;
    }

    std::lock_guard<std::mutex> resource_lock(filter->asset_mutex);
    if (!filter->atlas_texture || filter->frame_count == 0 ||
        !phase1 || !phase2) {
        set_cursor_hidden(filter);
        phase3_render(filter->phase3, effect);
        return;
    }

    const bool camera_active =
        phase1->current_zoom > 1.0005f ||
        !arzoom::nearly_equal(phase1->current_center,
                              {0.5f, 0.5f}, 0.0005f);
    const bool click_active =
        phase2->click_visual_enabled.load(std::memory_order_acquire) &&
        phase2->click_shader_ready && phase2->clicks.has_active();

    if (!phase1->effect_ready || !phase1->effect ||
        !phase1->enabled.load(std::memory_order_acquire) ||
        (!camera_active && !click_active && !cursor_active)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    if (!obs_source_process_filter_begin(
            phase1->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        obs_source_skip_video_filter(phase1->context);
        return;
    }

    gs_effect_set_float(phase1->zoom_param,
                        std::max(phase1->current_zoom, 1.0f));
    vec2 center;
    vec2_set(&center, phase1->current_center.x, phase1->current_center.y);
    gs_effect_set_vec2(phase1->center_param, &center);

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

    gs_effect_set_texture(filter->cursor_atlas_param, filter->atlas_texture);

    vec2 content;
    vec2_set(&content,
             filter->cursor_content_x.load(std::memory_order_acquire),
             filter->cursor_content_y.load(std::memory_order_acquire));
    gs_effect_set_vec2(filter->cursor_content_param, &content);

    vec2 asset_size;
    vec2_set(&asset_size,
             static_cast<float>(filter->frame_width),
             static_cast<float>(filter->frame_height));
    gs_effect_set_vec2(filter->cursor_asset_size_param, &asset_size);

    vec2 hotspot;
    vec2_set(&hotspot,
             filter->hotspot_x.load(std::memory_order_acquire),
             filter->hotspot_y.load(std::memory_order_acquire));
    gs_effect_set_vec2(filter->cursor_hotspot_param, &hotspot);

    vec2 atlas_grid;
    vec2_set(&atlas_grid,
             static_cast<float>(filter->atlas_columns),
             static_cast<float>(filter->atlas_rows));
    gs_effect_set_vec2(filter->cursor_atlas_grid_param, &atlas_grid);

    const int safe_frame = std::clamp(
        filter->current_frame.load(std::memory_order_acquire),
        0, static_cast<int>(filter->frame_count - 1));
    gs_effect_set_float(filter->cursor_frame_param,
                        static_cast<float>(safe_frame));
    gs_effect_set_float(filter->cursor_size_param,
                        filter->cursor_size_px.load(std::memory_order_acquire));
    gs_effect_set_float(filter->cursor_visible_param, 1.0f);

    obs_source_process_filter_end(
        phase1->context, phase1->effect, 0, 0);
}

void phase35_deactivate(void *data)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    if (!filter)
        return;
    phase3_deactivate(filter->phase3);
    if (Phase2Filter *phase2 = phase2_filter(filter)) {
        phase2->click_capture_for_cursor.store(false,
                                                std::memory_order_release);
    }
    filter->playback.reset();
    filter->current_frame.store(0, std::memory_order_release);
    filter->cursor_position_valid.store(false, std::memory_order_release);
}

void phase35_destroy(void *data)
{
    auto *filter = static_cast<Phase35Filter *>(data);
    if (!filter)
        return;

    gs_texture_t *texture = nullptr;
    {
        std::lock_guard<std::mutex> lock(filter->asset_mutex);
        texture = filter->atlas_texture;
        filter->atlas_texture = nullptr;
    }
    destroy_cursor_texture(texture);
    phase3_destroy(filter->phase3);
    delete filter;
}

void *phase35_create(obs_data_t *settings, obs_source_t *context)
{
    auto *phase3 = static_cast<Phase3Filter *>(
        phase3_create(settings, context));
    if (!phase3)
        return nullptr;

    auto *filter = new (std::nothrow) Phase35Filter();
    if (!filter) {
        phase3_destroy(phase3);
        return nullptr;
    }
    filter->phase3 = phase3;

    ArZoomFilter *phase1 = phase1_filter35(filter);
    if (phase1 && phase1->effect) {
        filter->cursor_atlas_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_atlas");
        filter->cursor_content_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_content_position");
        filter->cursor_asset_size_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_asset_size");
        filter->cursor_hotspot_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_hotspot");
        filter->cursor_atlas_grid_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_atlas_grid");
        filter->cursor_frame_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_frame");
        filter->cursor_size_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_size_px");
        filter->cursor_visible_param = gs_effect_get_param_by_name(
            phase1->effect, "cursor_visible");
        filter->cursor_shader_ready =
            filter->cursor_atlas_param && filter->cursor_content_param &&
            filter->cursor_asset_size_param && filter->cursor_hotspot_param &&
            filter->cursor_atlas_grid_param && filter->cursor_frame_param &&
            filter->cursor_size_param && filter->cursor_visible_param;
    }

    if (!filter->cursor_shader_ready) {
        blog(LOG_WARNING,
             "[ArZoom] Presentation Cursor shader parameters unavailable; "
             "v0.4.0 camera, clicks and presenter controls remain functional.");
    }

    phase35_update(filter, settings);
    blog(LOG_INFO, "[ArZoom] Phase 3.5 Presentation Cursor runtime ready");
    return filter;
}

/* Runs after the Phase 3 initializer in the included translation unit. */
struct Phase35SourceInfoOverride {
    Phase35SourceInfoOverride()
    {
        arzoom_filter_info.create = phase35_create;
        arzoom_filter_info.destroy = phase35_destroy;
        arzoom_filter_info.video_tick = phase35_tick;
        arzoom_filter_info.video_render = phase35_render;
        arzoom_filter_info.update = phase35_update;
        arzoom_filter_info.get_properties = phase35_properties;
        arzoom_filter_info.get_defaults = phase35_defaults;
        arzoom_filter_info.deactivate = phase35_deactivate;
    }
};

Phase35SourceInfoOverride phase35_source_info_override;

} // namespace
