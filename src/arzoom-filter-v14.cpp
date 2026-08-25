#include "arzoom-filter-v13.cpp"

/*
 * P4.1 Scene Camera intelligence scope
 * -------------------------------------
 * The presenter-aware viewport coordinator is intentionally enabled only for
 * the managed scene-level ArZoom Camera. Per-source ArZoom keeps the accepted
 * P1 SmartCamera behavior unchanged.
 */
namespace {

void phase41_scene_context_tick(void *data, float seconds)
{
    auto *filter = static_cast<Phase41Filter *>(data);
    if (!filter || !filter->phase4) {
        phase41_tick(data, seconds);
        return;
    }

    ArZoomFilter *phase1 = phase1_from_phase41(filter);
    obs_source_t *scene_source = nullptr;
    const bool managed_scene =
        is_managed_scene_camera(phase1, &scene_source);
    if (phase1)
        phase1->camera.set_scene_context(managed_scene);

    phase41_tick(filter, seconds);
}

struct Phase41SceneContextSourceInfoOverride {
    Phase41SceneContextSourceInfoOverride()
    {
        arzoom_filter_info.video_tick = phase41_scene_context_tick;
    }
};

Phase41SceneContextSourceInfoOverride phase41_scene_context_source_info_override;

} // namespace
