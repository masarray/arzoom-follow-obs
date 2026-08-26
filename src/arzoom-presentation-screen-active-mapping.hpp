#pragma once

#include "arzoom-presentation-screen-resolver.hpp"
#include "arzoom-presentation-screen-settings.hpp"

#include <cstddef>
#include <vector>

namespace arzoom {

struct PresentationScreenActiveMappingCandidate {
    PresentationScreenDiscoveredIdentity identity{};
    PresentationScreenRect physical_monitor{};
    SceneAxisAlignedMapping mapping{};

    bool visible = false;
    bool monitor_resolved = false;
    bool geometry_valid = false;
    bool runtime_ready = false;
};

struct PresentationScreenActiveMappingSet {
    PresentationScreenEligibilityResult eligibility{};
    std::vector<PresentationScreenCandidate> resolver_candidates;

    bool ready() const
    {
        return eligibility.ready() && eligibility.eligible_count > 0 &&
               resolver_candidates.size() == eligibility.eligible.size();
    }
};

inline PresentationScreenActiveMappingSet
presentation_screen_prepare_active_mapping(
    const PresentationScreenActiveMappingCandidate *candidates,
    std::size_t candidate_count,
    const PresentationScreenSelectionSettings &settings)
{
    PresentationScreenActiveMappingSet prepared;
    if (!candidates || candidate_count == 0) {
        prepared.eligibility = presentation_screen_resolve_eligibility(
            nullptr, 0, settings);
        return prepared;
    }

    prepared.resolver_candidates.resize(candidate_count);

    std::vector<PresentationScreenEligibilityCandidate> eligibility_candidates;
    eligibility_candidates.resize(candidate_count);

    for (std::size_t i = 0; i < candidate_count; ++i) {
        const auto &candidate = candidates[i];

        auto &eligibility_candidate = eligibility_candidates[i];
        eligibility_candidate.identity = candidate.identity;
        eligibility_candidate.ready = candidate.runtime_ready;

        auto &resolver_candidate = prepared.resolver_candidates[i];
        resolver_candidate.source_uuid = candidate.identity.source_uuid;
        resolver_candidate.physical_monitor = candidate.physical_monitor;
        resolver_candidate.mapping = candidate.mapping;
        resolver_candidate.visible = candidate.visible;
        resolver_candidate.source_resolved = candidate.identity.source_resolved;
        resolver_candidate.monitor_resolved = candidate.monitor_resolved;
        resolver_candidate.geometry_valid = candidate.geometry_valid;
    }

    prepared.eligibility = presentation_screen_resolve_eligibility(
        eligibility_candidates.data(), eligibility_candidates.size(), settings);

    if (prepared.eligibility.eligible.size() == candidate_count) {
        for (std::size_t i = 0; i < candidate_count; ++i) {
            prepared.resolver_candidates[i].eligible =
                prepared.eligibility.eligible[i];
        }
    }

    return prepared;
}

inline PresentationScreenResolveResult presentation_screen_resolve_active_mapping(
    const PresentationScreenActiveMappingSet &prepared,
    std::int64_t cursor_x,
    std::int64_t cursor_y)
{
    if (!prepared.ready())
        return {};

    return resolve_presentation_screen(
        prepared.resolver_candidates.data(),
        prepared.resolver_candidates.size(),
        cursor_x,
        cursor_y);
}

} // namespace arzoom
