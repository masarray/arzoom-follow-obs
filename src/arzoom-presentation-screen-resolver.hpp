#pragma once

#include "arzoom-scene-camera-core.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace arzoom {

struct PresentationScreenRect {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;

    bool valid() const
    {
        return right > left && bottom > top;
    }

    bool contains(std::int64_t x, std::int64_t y) const
    {
        return valid() && x >= left && x < right && y >= top && y < bottom;
    }
};

struct PresentationScreenCandidate {
    std::string source_uuid;
    PresentationScreenRect physical_monitor{};
    SceneAxisAlignedMapping mapping{};

    bool eligible = false;
    bool visible = false;
    bool source_resolved = false;
    bool monitor_resolved = false;
    bool geometry_valid = false;
};

enum class PresentationScreenResolveStatus {
    Active,
    NoEligibleScreens,
    CursorOutsideEligibleScreens,
    AmbiguousMonitorOwnership,
    ActiveScreenInvalid,
};

inline constexpr std::size_t kNoActivePresentationScreen =
    std::numeric_limits<std::size_t>::max();

struct PresentationScreenResolveResult {
    PresentationScreenResolveStatus status =
        PresentationScreenResolveStatus::NoEligibleScreens;
    std::size_t active_index = kNoActivePresentationScreen;

    bool active() const
    {
        return status == PresentationScreenResolveStatus::Active &&
               active_index != kNoActivePresentationScreen;
    }
};

inline PresentationScreenResolveResult resolve_presentation_screen(
    const PresentationScreenCandidate *candidates,
    std::size_t candidate_count,
    std::int64_t cursor_x,
    std::int64_t cursor_y)
{
    PresentationScreenResolveResult result;

    if (!candidates || candidate_count == 0)
        return result;

    bool has_eligible_screen = false;
    std::size_t owning_index = kNoActivePresentationScreen;
    std::size_t owning_count = 0;

    for (std::size_t i = 0; i < candidate_count; ++i) {
        const PresentationScreenCandidate &candidate = candidates[i];
        if (!candidate.eligible)
            continue;

        has_eligible_screen = true;

        if (!candidate.monitor_resolved ||
            !candidate.physical_monitor.contains(cursor_x, cursor_y)) {
            continue;
        }

        owning_index = i;
        ++owning_count;
        if (owning_count > 1) {
            result.status =
                PresentationScreenResolveStatus::AmbiguousMonitorOwnership;
            return result;
        }
    }

    if (!has_eligible_screen) {
        result.status = PresentationScreenResolveStatus::NoEligibleScreens;
        return result;
    }

    if (owning_count == 0) {
        result.status =
            PresentationScreenResolveStatus::CursorOutsideEligibleScreens;
        return result;
    }

    const PresentationScreenCandidate &active = candidates[owning_index];
    if (!active.visible || !active.source_resolved ||
        active.source_uuid.empty() || !active.geometry_valid ||
        !active.mapping.valid()) {
        result.status = PresentationScreenResolveStatus::ActiveScreenInvalid;
        return result;
    }

    result.status = PresentationScreenResolveStatus::Active;
    result.active_index = owning_index;
    return result;
}

} // namespace arzoom
