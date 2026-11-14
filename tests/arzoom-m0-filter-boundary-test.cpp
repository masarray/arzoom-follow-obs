#include "../src/arzoom-filter-boundary.hpp"
#include "../src/arzoom-scene-camera-core.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

void legacy_single_identity_is_frozen()
{
    require(arzoom::kSingleFilterId == "arzoom_filter",
            "legacy Single internal ID changed and would break saved OBS projects");
    require(arzoom::kSceneCameraFilterId == arzoom::kSingleFilterId,
            "managed Scene Camera stopped targeting the legacy Single ID");
    require(arzoom::kSingleFilterDisplayName == "ArZoom Single",
            "Single user-facing name drifted");
    require(arzoom::camera_filter_kind(arzoom::kSingleFilterId) ==
                arzoom::CameraFilterKind::Single,
            "legacy filter ID did not classify as Single");
}

void multi_identity_is_distinct()
{
    require(arzoom::kMultiFilterId == "arzoom_filter_multi",
            "Multi internal ID changed unexpectedly");
    require(arzoom::kMultiFilterId != arzoom::kSingleFilterId,
            "Single and Multi share an internal OBS ID");
    require(arzoom::kMultiFilterDisplayName == "ArZoom Multi",
            "Multi user-facing name drifted");
    require(arzoom::camera_filter_kind(arzoom::kMultiFilterId) ==
                arzoom::CameraFilterKind::Multi,
            "Multi filter ID did not classify as Multi");
    require(arzoom::camera_filter_kind("other_filter") ==
                arzoom::CameraFilterKind::Other,
            "foreign filter was misclassified as an ArZoom camera authority");
    require(arzoom::camera_filter_kind(static_cast<const char *>(nullptr)) ==
                arzoom::CameraFilterKind::Other,
            "null filter ID was not handled safely");
}

void authority_conflict_policy_is_conservative_and_bounded()
{
    arzoom::CameraAuthorityCounts none{};
    require(!none.conflict(), "empty source reported a camera conflict");

    arzoom::CameraAuthorityCounts single_only{1, 0};
    require(!single_only.conflict(), "one Single reported a camera conflict");

    arzoom::CameraAuthorityCounts multi_only{0, 1};
    require(!multi_only.conflict(), "one Multi reported a camera conflict");

    arzoom::CameraAuthorityCounts mixed{1, 1};
    require(mixed.conflict(), "Single + Multi did not report a conflict");

    arzoom::CameraAuthorityCounts duplicate_single{2, 0};
    require(duplicate_single.conflict(),
            "duplicate Single authorities did not report a conflict");

    arzoom::CameraAuthorityCounts duplicate_multi{0, 2};
    require(duplicate_multi.conflict(),
            "duplicate Multi authorities did not report a conflict");
}

} // namespace

int main()
{
    legacy_single_identity_is_frozen();
    multi_identity_is_distinct();
    authority_conflict_policy_is_conservative_and_bounded();
    std::cout << "ArZoom Multi M0 filter boundary gates: PASS\n";
    return 0;
}
