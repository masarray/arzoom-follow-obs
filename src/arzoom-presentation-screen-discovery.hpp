#pragma once

#include <cstddef>
#include <limits>
#include <string>

namespace arzoom {

struct PresentationScreenDiscoveredIdentity {
    std::string source_uuid;
    std::string display_label;
    bool source_resolved = false;

    bool valid() const
    {
        return source_resolved && !source_uuid.empty();
    }
};

inline bool presentation_screen_same_source_identity(
    const PresentationScreenDiscoveredIdentity &a,
    const PresentationScreenDiscoveredIdentity &b)
{
    return a.valid() && b.valid() && a.source_uuid == b.source_uuid;
}

enum class PresentationScreenIdentityLookupStatus {
    Found,
    NotFound,
    Ambiguous,
};

inline constexpr std::size_t kNoPresentationScreenIdentity =
    std::numeric_limits<std::size_t>::max();

struct PresentationScreenIdentityLookupResult {
    PresentationScreenIdentityLookupStatus status =
        PresentationScreenIdentityLookupStatus::NotFound;
    std::size_t index = kNoPresentationScreenIdentity;

    bool found() const
    {
        return status == PresentationScreenIdentityLookupStatus::Found &&
               index != kNoPresentationScreenIdentity;
    }
};

inline PresentationScreenIdentityLookupResult
presentation_screen_find_discovered_identity(
    const PresentationScreenDiscoveredIdentity *identities,
    std::size_t identity_count,
    const std::string &source_uuid)
{
    PresentationScreenIdentityLookupResult result;
    if (!identities || identity_count == 0 || source_uuid.empty())
        return result;

    for (std::size_t i = 0; i < identity_count; ++i) {
        const auto &identity = identities[i];
        if (!identity.valid() || identity.source_uuid != source_uuid)
            continue;

        if (result.index != kNoPresentationScreenIdentity) {
            result.status = PresentationScreenIdentityLookupStatus::Ambiguous;
            result.index = kNoPresentationScreenIdentity;
            return result;
        }
        result.index = i;
    }

    if (result.index != kNoPresentationScreenIdentity)
        result.status = PresentationScreenIdentityLookupStatus::Found;
    return result;
}

} // namespace arzoom
