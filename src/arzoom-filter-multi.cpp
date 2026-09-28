#include "arzoom-filter-boundary.hpp"

#include <obs-module.h>

#include <atomic>
#include <new>

namespace {

struct ArZoomMultiFilter {
    obs_source_t *context = nullptr;
    obs_weak_source_t *parent_weak = nullptr;
    std::atomic<bool> authority_conflict{false};
};

void refresh_authority_conflict(ArZoomMultiFilter *filter);

void parent_filter_event(void *data, calldata_t *)
{
    refresh_authority_conflict(static_cast<ArZoomMultiFilter *>(data));
}

void disconnect_parent_signals(ArZoomMultiFilter *filter)
{
    if (!filter || !filter->parent_weak)
        return;

    obs_source_t *parent = obs_weak_source_get_source(filter->parent_weak);
    if (parent) {
        signal_handler_t *handler = obs_source_get_signal_handler(parent);
        if (handler) {
            signal_handler_disconnect(handler, "filter_add",
                                      parent_filter_event, filter);
            signal_handler_disconnect(handler, "filter_remove",
                                      parent_filter_event, filter);
        }
        obs_source_release(parent);
    }

    obs_weak_source_release(filter->parent_weak);
    filter->parent_weak = nullptr;
}

struct AuthorityScan {
    arzoom::CameraAuthorityCounts counts{};
};

void scan_authority_filter(obs_source_t *, obs_source_t *child, void *param)
{
    auto *scan = static_cast<AuthorityScan *>(param);
    if (!scan || !child)
        return;

    switch (arzoom::camera_filter_kind(obs_source_get_id(child))) {
    case arzoom::CameraFilterKind::Single:
        ++scan->counts.single;
        break;
    case arzoom::CameraFilterKind::Multi:
        ++scan->counts.multi;
        break;
    case arzoom::CameraFilterKind::Other:
        break;
    }
}

void refresh_authority_conflict(ArZoomMultiFilter *filter)
{
    if (!filter || !filter->parent_weak)
        return;

    obs_source_t *parent = obs_weak_source_get_source(filter->parent_weak);
    if (!parent) {
        filter->authority_conflict.store(false, std::memory_order_release);
        return;
    }

    AuthorityScan scan;
    obs_source_enum_filters(parent, scan_authority_filter, &scan);
    obs_source_release(parent);

    const bool next = scan.counts.conflict();
    const bool previous = filter->authority_conflict.exchange(
        next, std::memory_order_acq_rel);
    if (next == previous)
        return;

    if (next) {
        blog(LOG_WARNING,
             "[ArZoom Multi] Camera authority conflict: source has %zu ArZoom Single and %zu ArZoom Multi filter(s). Multi remains pass-through.",
             scan.counts.single, scan.counts.multi);
    } else {
        blog(LOG_INFO,
             "[ArZoom Multi] Camera authority conflict cleared; M0 remains pass-through by design");
    }
}

void multi_filter_add(void *data, obs_source_t *source)
{
    auto *filter = static_cast<ArZoomMultiFilter *>(data);
    if (!filter || !source)
        return;

    disconnect_parent_signals(filter);
    filter->parent_weak = obs_source_get_weak_source(source);
    if (!filter->parent_weak)
        return;

    signal_handler_t *handler = obs_source_get_signal_handler(source);
    if (handler) {
        signal_handler_connect(handler, "filter_add", parent_filter_event,
                               filter);
        signal_handler_connect(handler, "filter_remove", parent_filter_event,
                               filter);
    }

    refresh_authority_conflict(filter);
    if (!filter->authority_conflict.load(std::memory_order_acquire)) {
        blog(LOG_INFO,
             "[ArZoom Multi] M0 attached with a single camera authority; canonical mapping is intentionally not wired yet");
    }
}

void multi_filter_remove(void *data, obs_source_t *)
{
    auto *filter = static_cast<ArZoomMultiFilter *>(data);
    if (!filter)
        return;

    disconnect_parent_signals(filter);
    filter->authority_conflict.store(false, std::memory_order_release);
}

const char *multi_filter_name(void *)
{
    return arzoom::kMultiFilterDisplayName.data();
}

void *multi_create(obs_data_t *, obs_source_t *context)
{
    auto *filter = new (std::nothrow) ArZoomMultiFilter();
    if (!filter)
        return nullptr;

    filter->context = context;
    blog(LOG_INFO,
         "[ArZoom Multi] M0 pass-through filter created; no Multi mapping/camera runtime is active");
    return filter;
}

void multi_destroy(void *data)
{
    auto *filter = static_cast<ArZoomMultiFilter *>(data);
    if (!filter)
        return;

    disconnect_parent_signals(filter);
    delete filter;
}

void multi_render(void *data, gs_effect_t *)
{
    auto *filter = static_cast<ArZoomMultiFilter *>(data);
    if (!filter || !filter->context)
        return;

    /* M0 is deliberately behavior-neutral.  OBS renders the unmodified parent
     * source while registration/lifecycle/conflict boundaries are proven. */
    obs_source_skip_video_filter(filter->context);
}

} // namespace

obs_source_info arzoom_multi_filter_info = {};

namespace {
struct ArZoomMultiSourceInfoInitializer {
    ArZoomMultiSourceInfoInitializer()
    {
        arzoom_multi_filter_info.id = arzoom::kMultiFilterId.data();
        arzoom_multi_filter_info.type = OBS_SOURCE_TYPE_FILTER;
        arzoom_multi_filter_info.output_flags =
            OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_DONT_SHOW_PROPERTIES;
        arzoom_multi_filter_info.get_name = multi_filter_name;
        arzoom_multi_filter_info.create = multi_create;
        arzoom_multi_filter_info.destroy = multi_destroy;
        arzoom_multi_filter_info.video_render = multi_render;
        arzoom_multi_filter_info.filter_add = multi_filter_add;
        arzoom_multi_filter_info.filter_remove = multi_filter_remove;
        arzoom_multi_filter_info.icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE;
    }
};

ArZoomMultiSourceInfoInitializer arzoom_multi_source_info_initializer;
} // namespace
