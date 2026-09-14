#pragma once

#include <cstddef>
#include <string_view>

namespace arzoom {

/* M0 product boundary.  The legacy ID is a compatibility contract: existing
 * OBS projects already persist it and must continue to load as ArZoom Single. */
inline constexpr std::string_view kSingleFilterId = "arzoom_filter";
inline constexpr std::string_view kMultiFilterId = "arzoom_filter_multi";
inline constexpr std::string_view kSingleFilterDisplayName = "ArZoom Single";
inline constexpr std::string_view kMultiFilterDisplayName = "ArZoom Multi";

enum class CameraFilterKind {
    Other,
    Single,
    Multi,
};

inline constexpr CameraFilterKind camera_filter_kind(std::string_view id)
{
    return id == kSingleFilterId
               ? CameraFilterKind::Single
               : (id == kMultiFilterId ? CameraFilterKind::Multi
                                       : CameraFilterKind::Other);
}

inline CameraFilterKind camera_filter_kind(const char *id)
{
    return id ? camera_filter_kind(std::string_view{id})
              : CameraFilterKind::Other;
}

/* M0 treats attached camera filters as authorities conservatively.  Runtime
 * enable/settings semantics stay out of this boundary milestone; duplicate or
 * mixed camera filters are reported rather than auto-mutated. */
struct CameraAuthorityCounts {
    std::size_t single = 0;
    std::size_t multi = 0;

    constexpr bool conflict() const
    {
        return single > 1 || multi > 1 || (single > 0 && multi > 0);
    }
};

} // namespace arzoom
