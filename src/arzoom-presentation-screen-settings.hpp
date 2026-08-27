#pragma once

#include "arzoom-presentation-screen-discovery.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace arzoom {

inline constexpr int kPresentationScreenSettingsSchemaVersion = 1;

inline constexpr const char kPresentationScreenSettingsSchemaKey[] =
    "p42_presentation_screens_schema";
inline constexpr const char kPresentationScreenSettingsArrayKey[] =
    "p42_presentation_screens";
inline constexpr const char kPresentationScreenSettingsUuidKey[] =
    "source_uuid";

struct PresentationScreenSelectionWire {
    int schema_version = kPresentationScreenSettingsSchemaVersion;
    bool persisted = false;
    std::vector<std::string> source_uuids;
};

struct PresentationScreenSelectionSettings {
    int schema_version = kPresentationScreenSettingsSchemaVersion;
    bool persisted = false;
    std::vector<std::string> selected_source_uuids;
};

inline std::vector<std::string> presentation_screen_normalize_selected_uuids(
    const std::vector<std::string> &source_uuids)
{
    std::vector<std::string> normalized;
    normalized.reserve(source_uuids.size());
    for (const auto &uuid : source_uuids) {
        if (uuid.empty())
            continue;
        if (std::find(normalized.begin(), normalized.end(), uuid) ==
            normalized.end()) {
            normalized.push_back(uuid);
        }
    }
    return normalized;
}

inline PresentationScreenSelectionWire presentation_screen_encode_selection(
    const PresentationScreenSelectionSettings &settings)
{
    PresentationScreenSelectionWire wire;
    wire.schema_version = settings.schema_version;
    wire.persisted = settings.persisted;
    wire.source_uuids = presentation_screen_normalize_selected_uuids(
        settings.selected_source_uuids);
    return wire;
}

inline PresentationScreenSelectionSettings presentation_screen_decode_selection(
    const PresentationScreenSelectionWire &wire)
{
    PresentationScreenSelectionSettings settings;
    settings.schema_version = wire.schema_version;
    settings.persisted = wire.persisted;
    settings.selected_source_uuids =
        presentation_screen_normalize_selected_uuids(wire.source_uuids);
    return settings;
}

inline bool presentation_screen_selection_contains(
    const PresentationScreenSelectionSettings &settings,
    const std::string &source_uuid)
{
    if (source_uuid.empty())
        return false;
    return std::find(settings.selected_source_uuids.begin(),
                     settings.selected_source_uuids.end(),
                     source_uuid) != settings.selected_source_uuids.end();
}

inline PresentationScreenSelectionSettings presentation_screen_set_selected(
    const PresentationScreenSelectionSettings &settings,
    const std::string &source_uuid,
    bool selected)
{
    PresentationScreenSelectionSettings next = settings;
    next.schema_version = kPresentationScreenSettingsSchemaVersion;
    next.persisted = true;
    next.selected_source_uuids = presentation_screen_normalize_selected_uuids(
        next.selected_source_uuids);

    if (source_uuid.empty())
        return next;

    const auto found = std::find(next.selected_source_uuids.begin(),
                                 next.selected_source_uuids.end(),
                                 source_uuid);
    if (selected) {
        if (found == next.selected_source_uuids.end())
            next.selected_source_uuids.push_back(source_uuid);
    } else if (found != next.selected_source_uuids.end()) {
        next.selected_source_uuids.erase(found);
    }
    return next;
}

inline PresentationScreenSelectionSettings presentation_screen_replace_selection(
    const std::vector<std::string> &source_uuids)
{
    PresentationScreenSelectionSettings settings;
    settings.schema_version = kPresentationScreenSettingsSchemaVersion;
    settings.persisted = true;
    settings.selected_source_uuids =
        presentation_screen_normalize_selected_uuids(source_uuids);
    return settings;
}

struct PresentationScreenEligibilityCandidate {
    PresentationScreenDiscoveredIdentity identity{};
    bool ready = false;
};

enum class PresentationScreenEligibilityStatus {
    Unavailable,
    AutoSingle,
    ReadySelected,
    NeedsSetup,
    SelectedScreensUnavailable,
    AmbiguousIdentity,
    UnsupportedSchema,
};

struct PresentationScreenEligibilityResult {
    PresentationScreenEligibilityStatus status =
        PresentationScreenEligibilityStatus::Unavailable;
    std::vector<bool> eligible;
    std::size_t eligible_count = 0;
    std::size_t unavailable_selected_count = 0;
    bool implicit_auto = false;

    bool ready() const
    {
        return status == PresentationScreenEligibilityStatus::AutoSingle ||
               status == PresentationScreenEligibilityStatus::ReadySelected;
    }
};

inline PresentationScreenEligibilityResult presentation_screen_resolve_eligibility(
    const PresentationScreenEligibilityCandidate *candidates,
    std::size_t candidate_count,
    const PresentationScreenSelectionSettings &settings)
{
    PresentationScreenEligibilityResult result;
    result.eligible.assign(candidate_count, false);

    if (!candidates || candidate_count == 0) {
        result.status = settings.persisted
                            ? PresentationScreenEligibilityStatus::SelectedScreensUnavailable
                            : PresentationScreenEligibilityStatus::Unavailable;
        return result;
    }

    if (settings.persisted &&
        settings.schema_version != kPresentationScreenSettingsSchemaVersion) {
        result.status = PresentationScreenEligibilityStatus::UnsupportedSchema;
        return result;
    }

    if (!settings.persisted) {
        std::size_t ready_index = candidate_count;
        std::size_t ready_count = 0;
        for (std::size_t i = 0; i < candidate_count; ++i) {
            if (!candidates[i].ready || !candidates[i].identity.valid())
                continue;
            ready_index = i;
            ++ready_count;
        }

        if (ready_count == 0) {
            result.status = PresentationScreenEligibilityStatus::Unavailable;
            return result;
        }
        if (ready_count > 1) {
            result.status = PresentationScreenEligibilityStatus::NeedsSetup;
            return result;
        }

        result.eligible[ready_index] = true;
        result.eligible_count = 1;
        result.implicit_auto = true;
        result.status = PresentationScreenEligibilityStatus::AutoSingle;
        return result;
    }

    const auto selected =
        presentation_screen_normalize_selected_uuids(settings.selected_source_uuids);
    if (selected.empty()) {
        result.status = PresentationScreenEligibilityStatus::NeedsSetup;
        return result;
    }

    for (const auto &selected_uuid : selected) {
        std::size_t match_index = candidate_count;
        std::size_t match_count = 0;
        for (std::size_t i = 0; i < candidate_count; ++i) {
            const auto &identity = candidates[i].identity;
            if (!identity.valid() || identity.source_uuid != selected_uuid)
                continue;
            match_index = i;
            ++match_count;
        }

        if (match_count > 1) {
            result.status = PresentationScreenEligibilityStatus::AmbiguousIdentity;
            std::fill(result.eligible.begin(), result.eligible.end(), false);
            result.eligible_count = 0;
            return result;
        }
        if (match_count == 0 || !candidates[match_index].ready) {
            ++result.unavailable_selected_count;
            continue;
        }
        if (!result.eligible[match_index]) {
            result.eligible[match_index] = true;
            ++result.eligible_count;
        }
    }

    result.status = result.eligible_count > 0
                        ? PresentationScreenEligibilityStatus::ReadySelected
                        : PresentationScreenEligibilityStatus::SelectedScreensUnavailable;
    return result;
}

} // namespace arzoom
