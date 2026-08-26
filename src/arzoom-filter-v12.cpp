#include "arzoom-filter-v11.cpp"
#include "arzoom-presentation-screen-discovery.hpp"
#include "arzoom-scene-mapping-runtime.hpp"

#include <limits>
#include <string>
#include <vector>

/*
 * Phase 4.1 generalized read-only scene mapping
 * ----------------------------------------------
 * Keep the accepted scene-level filter architecture.  This wrapper extends the
 * P4 coordinate owner from fullscreen-only to one visible top-level Display
 * Capture with a positive axis-aligned scale/inset transform and optional crop.
 *
 * P4.2 Slice 3 now discovers every visible top-level Display Capture as a
 * UUID-identified candidate with independently proven mapping metadata.  The
 * active runtime gate deliberately remains P4.1-compatible in this slice:
 * exactly one discovered candidate is required before pointer mapping is
 * installed.  Multi-screen active selection is a later slice.
 *
 * No scene-item transform is ever written.  Rotation/skew/flips, bounds modes,
 * nested ownership and multiple active Display Captures remain fail-safe until
 * their coordinate contracts are independently proven.
 */
namespace {

constexpr float kPhase41MappingRefreshSeconds = 0.25f;
constexpr float kSyntheticMappingTolerance = 0.0025f;

struct Phase41Filter {
    Phase4SceneFilter *phase4 = nullptr;
    float mapping_refresh_elapsed = 1.0f;

    bool layout_mapping_valid = false;
    bool mapping_warning_logged = false;
    std::string mapping_reason;

    arzoom::SceneAxisAlignedMapping mapping{};
    MonitorDescriptor physical_monitor{};
    MonitorDescriptor mapped_monitor{};
};

Phase351Filter *phase351_from_phase41(Phase41Filter *filter)
{
    Phase352Filter *phase352 =
        filter && filter->phase4 ? phase352_from_phase4(filter->phase4) : nullptr;
    return phase352 ? phase352->phase351 : nullptr;
}

ArZoomFilter *phase1_from_phase41(Phase41Filter *filter)
{
    return filter && filter->phase4 ? phase1_from_phase4(filter->phase4)
                                    : nullptr;
}

bool build_mapped_monitor(const MonitorDescriptor &physical,
                          const arzoom::SceneAxisAlignedMapping &mapping,
                          MonitorDescriptor &mapped)
{
    if (!physical.valid() || !mapping.valid())
        return false;

    const arzoom::SceneDesktopRect physical_rect{
        static_cast<std::int64_t>(physical.left),
        static_cast<std::int64_t>(physical.top),
        static_cast<std::int64_t>(physical.right),
        static_cast<std::int64_t>(physical.bottom),
    };
    arzoom::SceneDesktopRect mapped_rect;
    if (!arzoom::scene_mapping_build_synthetic_desktop_rect(
            physical_rect,
            mapping,
            static_cast<std::int64_t>(std::numeric_limits<long>::lowest()),
            static_cast<std::int64_t>(std::numeric_limits<long>::max()),
            mapped_rect)) {
        return false;
    }

    mapped = physical;
    mapped.left = static_cast<long>(mapped_rect.left);
    mapped.top = static_cast<long>(mapped_rect.top);
    mapped.right = static_cast<long>(mapped_rect.right);
    mapped.bottom = static_cast<long>(mapped_rect.bottom);
    mapped.label = physical.label + "  ·  ArZoom scene-mapped";
    return mapped.valid();
}

struct SceneDisplayCandidateSnapshot {
    arzoom::PresentationScreenDiscoveredIdentity identity{};
    arzoom::SceneAxisAlignedMapping mapping{};
    MonitorDescriptor physical_monitor{};
    MonitorDescriptor mapped_monitor{};
    bool visible = false;
    bool geometry_valid = false;
    bool monitor_resolved = false;
    bool native_cursor_enabled = false;
    std::string reason;

    bool ready() const
    {
        return visible && identity.valid() && geometry_valid &&
               monitor_resolved && mapping.valid() &&
               physical_monitor.valid() && mapped_monitor.valid();
    }
};

struct SceneDisplayDiscoveryContext {
    float canvas_width = 0.0f;
    float canvas_height = 0.0f;
    std::vector<SceneDisplayCandidateSnapshot> *candidates = nullptr;
};

void push_discovery_failure(SceneDisplayDiscoveryContext *context,
                            SceneDisplayCandidateSnapshot candidate,
                            const char *reason)
{
    if (!context || !context->candidates)
        return;
    candidate.reason = reason ? reason : "Display Capture candidate is unavailable";
    context->candidates->push_back(std::move(candidate));
}

bool discover_scene_display_candidate_cb(obs_scene_t *,
                                         obs_sceneitem_t *item,
                                         void *param)
{
    auto *context = static_cast<SceneDisplayDiscoveryContext *>(param);
    if (!context || !context->candidates || !item ||
        !obs_sceneitem_visible(item)) {
        return true;
    }

    obs_source_t *source = obs_sceneitem_get_source(item);
    if (!is_display_capture(source))
        return true;

    SceneDisplayCandidateSnapshot candidate;
    candidate.visible = true;

    const char *display_name = obs_source_get_name(source);
    candidate.identity.display_label = display_name ? display_name : "";

    const char *uuid = obs_source_get_uuid(source);
    if (!uuid || !*uuid) {
        push_discovery_failure(context, std::move(candidate),
                               "Display Capture source UUID is unavailable");
        return true;
    }
    candidate.identity.source_uuid = uuid;

    obs_source_t *resolved_source = obs_get_source_by_uuid(uuid);
    if (!resolved_source) {
        push_discovery_failure(context, std::move(candidate),
                               "Display Capture source UUID could not be resolved");
        return true;
    }
    const bool source_identity_matches =
        resolved_source == source && is_display_capture(resolved_source);
    obs_source_release(resolved_source);
    if (!source_identity_matches) {
        push_discovery_failure(context, std::move(candidate),
                               "Display Capture source UUID resolved to a different source");
        return true;
    }
    candidate.identity.source_resolved = true;

    if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
        push_discovery_failure(
            context, std::move(candidate),
            "scene-item bounds scaling is not supported yet; use normal scale/inset transform");
        return true;
    }

    const float source_width =
        static_cast<float>(obs_source_get_width(source));
    const float source_height =
        static_cast<float>(obs_source_get_height(source));

    matrix4 transform;
    obs_sceneitem_get_box_transform(item, &transform);
    const arzoom::SceneMappingQuad quad{
        transformed_box_corner(transform, 0.0f, 0.0f),
        transformed_box_corner(transform, 1.0f, 0.0f),
        transformed_box_corner(transform, 0.0f, 1.0f),
        transformed_box_corner(transform, 1.0f, 1.0f),
    };

    obs_sceneitem_crop crop{};
    obs_sceneitem_get_crop(item, &crop);
    const arzoom::SceneDisplayGeometrySnapshot geometry{
        quad,
        context->canvas_width,
        context->canvas_height,
        source_width,
        source_height,
        static_cast<float>(crop.left),
        static_cast<float>(crop.top),
        static_cast<float>(crop.right),
        static_cast<float>(crop.bottom),
    };
    const auto mapping_result =
        arzoom::scene_mapping_build_display_geometry(geometry);
    if (!mapping_result.ok()) {
        candidate.reason = std::string(
            arzoom::scene_mapping_status_text(mapping_result.status));
        context->candidates->push_back(std::move(candidate));
        return true;
    }
    candidate.mapping = mapping_result.mapping;
    candidate.geometry_valid = true;

    if (!monitor_from_capture_source(source, candidate.physical_monitor)) {
        push_discovery_failure(
            context, std::move(candidate),
            "Display Capture monitor could not be resolved deterministically");
        return true;
    }
    candidate.monitor_resolved = true;

    if (!build_mapped_monitor(candidate.physical_monitor,
                              candidate.mapping,
                              candidate.mapped_monitor)) {
        push_discovery_failure(
            context, std::move(candidate),
            "scene mapping exceeded safe desktop-coordinate range");
        return true;
    }

    candidate.native_cursor_enabled = capture_cursor_enabled(source);
    candidate.reason.clear();
    context->candidates->push_back(std::move(candidate));
    return true;
}

bool discover_scene_display_candidates(
    obs_source_t *scene_source,
    std::vector<SceneDisplayCandidateSnapshot> &candidates,
    std::string &reason)
{
    obs_scene_t *scene = scene_source ? obs_scene_from_source(scene_source)
                                      : nullptr;
    if (!scene) {
        reason = "filter target is not an OBS scene";
        return false;
    }

    candidates.clear();
    SceneDisplayDiscoveryContext context;
    context.canvas_width =
        static_cast<float>(obs_source_get_width(scene_source));
    context.canvas_height =
        static_cast<float>(obs_source_get_height(scene_source));
    context.candidates = &candidates;
    obs_scene_enum_items(scene, discover_scene_display_candidate_cb, &context);

    if (candidates.empty()) {
        reason = "no visible top-level Display Capture";
        return false;
    }

    reason.clear();
    return true;
}

bool resolve_phase41_layout(Phase41Filter *filter,
                            obs_source_t *scene_source)
{
    if (!filter || !scene_source)
        return false;

    std::vector<SceneDisplayCandidateSnapshot> candidates;
    std::string reason;
    if (!discover_scene_display_candidates(scene_source, candidates, reason)) {
        filter->mapping_reason = reason;
        return false;
    }

    /* Slice 3 proves candidate discovery only. Preserve P4.1 runtime ownership:
     * multiple visible Display Captures remain unavailable until the explicit
     * Presentation Screens eligibility model and active resolver are wired. */
    if (candidates.size() > 1) {
        filter->mapping_reason =
            "multiple visible top-level Display Captures (P4.2 target selection required)";
        return false;
    }

    const SceneDisplayCandidateSnapshot &candidate = candidates.front();
    if (!candidate.ready()) {
        filter->mapping_reason = candidate.reason.empty()
                                     ? "Display Capture candidate is unavailable"
                                     : candidate.reason;
        return false;
    }

    filter->mapping = candidate.mapping;
    filter->physical_monitor = candidate.physical_monitor;
    filter->mapped_monitor = candidate.mapped_monitor;
    filter->mapping_reason.clear();

    if (candidate.native_cursor_enabled &&
        !filter->phase4->nested_cursor_warning_logged) {
        blog(LOG_WARNING,
             "[ArZoom] Scene Camera: Display Capture native cursor is enabled. "
             "Turn it off when using an ArZoom Presentation Cursor to avoid double cursor.");
        filter->phase4->nested_cursor_warning_logged = true;
    } else if (!candidate.native_cursor_enabled) {
        filter->phase4->nested_cursor_warning_logged = false;
    }

    return true;
}

bool phase41_pointer_is_mappable(const Phase41Filter *filter)
{
    if (!filter || !filter->layout_mapping_valid ||
        !filter->physical_monitor.valid() || !filter->mapped_monitor.valid()) {
        return false;
    }

    long cursor_x = 0;
    long cursor_y = 0;
    if (!get_cursor_position(cursor_x, cursor_y) ||
        !cursor_in_monitor(filter->physical_monitor, cursor_x, cursor_y)) {
        return false;
    }

    const arzoom::Vec2 source_uv = cursor_normalized(
        filter->physical_monitor, cursor_x, cursor_y);
    if (!arzoom::scene_mapping_source_visible(filter->mapping, source_uv))
        return false;

    const arzoom::Vec2 expected_scene =
        arzoom::scene_mapping_source_to_scene(filter->mapping, source_uv);
    if (!arzoom::scene_mapping_scene_visible(expected_scene))
        return false;

    /* The inherited P1/P2/P3/P3.5 pipeline intentionally remains untouched.
     * It consumes MonitorDescriptor normalization for Smart Camera, click rings
     * and Presentation Cursor.  The synthetic descriptor makes that existing
     * normalization equal the scene-space affine mapping.  Guard rounding here
     * so an extreme layout can never silently produce a guessed coordinate. */
    if (!cursor_in_monitor(filter->mapped_monitor, cursor_x, cursor_y))
        return false;
    const arzoom::Vec2 inherited_scene = cursor_normalized(
        filter->mapped_monitor, cursor_x, cursor_y);
    return std::fabs(inherited_scene.x - expected_scene.x) <=
               kSyntheticMappingTolerance &&
           std::fabs(inherited_scene.y - expected_scene.y) <=
               kSyntheticMappingTolerance;
}

void phase41_install_mapping_state(Phase41Filter *filter,
                                   obs_source_t *scene_source)
{
    ArZoomFilter *phase1 = phase1_from_phase41(filter);
    if (!filter || !phase1 || !scene_source)
        return;

    phase1->monitor_refresh_elapsed = 0.0f;
    phase1->last_source_width = std::max(obs_source_get_width(scene_source), 1u);
    phase1->last_source_height = std::max(obs_source_get_height(scene_source), 1u);
    phase1->monitor_dirty.store(false, std::memory_order_release);

    const bool pointer_valid = phase41_pointer_is_mappable(filter);
    phase1->monitor_valid = pointer_valid;
    filter->phase4->mapping_valid = pointer_valid;
    if (filter->layout_mapping_valid)
        phase1->monitor = filter->mapped_monitor;
}

void phase41_refresh_mapping(Phase41Filter *filter,
                             obs_source_t *scene_source)
{
    if (!filter)
        return;

    const bool was_valid = filter->layout_mapping_valid;
    filter->layout_mapping_valid = resolve_phase41_layout(filter, scene_source);

    if (filter->layout_mapping_valid) {
        filter->mapping_warning_logged = false;
        filter->phase4->mapping_warning_logged = false;
        if (!was_valid) {
            blog(LOG_INFO,
                 "[ArZoom] P4.1 Scene Camera mapping proven: axis-aligned Display Capture "
                 "scale/inset/crop -> scene canvas (%s)",
                 filter->physical_monitor.label.c_str());
        }
    } else if (!filter->mapping_warning_logged) {
        blog(LOG_WARNING,
             "[ArZoom] P4.1 Scene Camera pointer mapping unavailable: %s. "
             "Camera zoom/freeze/overview remain safe; ArZoom will not guess coordinates.",
             filter->mapping_reason.empty() ? "unsupported scene layout"
                                            : filter->mapping_reason.c_str());
        filter->mapping_warning_logged = true;
    }

    phase41_install_mapping_state(filter, scene_source);
}

void phase41_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter || !filter->phase4 || !filter->phase4->phase354)
        return;

    ArZoomFilter *phase1 = phase1_from_phase41(filter);
    obs_source_t *scene_source = nullptr;
    if (!is_managed_scene_camera(phase1, &scene_source)) {
        phase4_tick(filter->phase4, seconds);
        return;
    }

    filter->mapping_refresh_elapsed += std::clamp(seconds, 0.0f, 0.10f);
    if (filter->mapping_refresh_elapsed >= kPhase41MappingRefreshSeconds) {
        filter->mapping_refresh_elapsed = 0.0f;
        phase41_refresh_mapping(filter, scene_source);
    } else {
        phase41_install_mapping_state(filter, scene_source);
    }

    phase354_tick(filter->phase4->phase354, seconds);
}

void phase41_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter || !filter->phase4)
        return;
    phase4_update(filter->phase4, settings);
    filter->mapping_refresh_elapsed = 1.0f;
}

obs_properties_t *phase41_properties(void *data)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    return phase4_properties(filter ? filter->phase4 : nullptr);
}

void phase41_render(void *data, gs_effect_t *effect)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter || !filter->phase4)
        return;

    prime_cursor_sampler_safety(filter->phase4);

    /* Make Presentation Cursor size ownership explicit at the final wrapper.
     * phase35_render owns the shared camera/click/cursor GPU pass.  Store the
     * configured base size here; the P4.1 shader applies the exact same live
     * zoom_amount used to sample the scene, so Zoom +/- cannot drift from the
     * image magnification. */
    Phase351Filter *phase351 = phase351_from_phase41(filter);
    Phase35Filter *phase35 = phase35_from_phase4(filter->phase4);
    if (phase351 && phase35) {
        phase35->cursor_size_px.store(
            phase351->base_cursor_size_px.load(std::memory_order_acquire),
            std::memory_order_release);
        phase35_render(phase35, effect);
        return;
    }

    phase4_render(filter->phase4, effect);
}

void phase41_deactivate(void *data)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (filter && filter->phase4)
        phase4_deactivate(filter->phase4);
}

void phase41_destroy(void *data)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter)
        return;
    phase4_destroy(filter->phase4);
    delete filter;
}

void *phase41_create(obs_data_t *settings, obs_source_t *context)
{
    /* Construct the P4 internals directly rather than calling phase4_create(),
     * which would emit the old fullscreen-only warning before P4.1 gets a
     * chance to prove a valid inset mapping. */
    auto *phase354 = static_cast<Phase354Filter *>(
        phase354_create(settings, context));
    if (!phase354)
        return nullptr;

    auto *phase4 = new (std::nothrow) Phase4SceneFilter();
    if (!phase4) {
        phase354_destroy(phase354);
        return nullptr;
    }
    phase4->phase354 = phase354;
    phase4->cursor_fallback_texture = create_transparent_cursor_fallback();
    if (!phase4->cursor_fallback_texture) {
        blog(LOG_WARNING,
             "[ArZoom] Could not allocate transparent cursor safety texture; "
             "first-pass sampler protection is unavailable.");
    }

    auto *filter = new (std::nothrow) Phase41Filter();
    if (!filter) {
        phase4_destroy(phase4);
        return nullptr;
    }
    filter->phase4 = phase4;

    ArZoomFilter *phase1 = phase1_from_phase41(filter);
    obs_source_t *scene_source = nullptr;
    if (is_managed_scene_camera(phase1, &scene_source)) {
        phase41_refresh_mapping(filter, scene_source);
        filter->mapping_refresh_elapsed = 0.0f;
        blog(LOG_INFO,
             "[ArZoom] Phase 4.1 read-only Scene Camera mapping runtime ready on scene '%s'",
             obs_source_get_name(scene_source));
    }
    return filter;
}

struct Phase41SourceInfoOverride {
    Phase41SourceInfoOverride()
    {
        arzoom_filter_info.create = phase41_create;
        arzoom_filter_info.destroy = phase41_destroy;
        arzoom_filter_info.video_tick = phase41_tick;
        arzoom_filter_info.video_render = phase41_render;
        arzoom_filter_info.update = phase41_update;
        arzoom_filter_info.get_properties = phase41_properties;
        arzoom_filter_info.get_defaults = phase352_defaults;
        arzoom_filter_info.deactivate = phase41_deactivate;
    }
};

Phase41SourceInfoOverride phase41_source_info_override;

} // namespace
