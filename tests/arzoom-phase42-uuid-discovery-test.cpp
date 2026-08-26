#include "../src/arzoom-presentation-screen-discovery.hpp"

#include <array>
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

void two_candidates_resolve_independently_by_uuid()
{
    using namespace arzoom;
    const std::array identities{
        PresentationScreenDiscoveredIdentity{"uuid-coding", "Coding", true},
        PresentationScreenDiscoveredIdentity{"uuid-app", "Application", true},
    };

    const auto coding = presentation_screen_find_discovered_identity(
        identities.data(), identities.size(), "uuid-coding");
    const auto app = presentation_screen_find_discovered_identity(
        identities.data(), identities.size(), "uuid-app");
    require(coding.found() && coding.index == 0,
            "coding candidate did not resolve by UUID");
    require(app.found() && app.index == 1,
            "application candidate did not resolve by UUID");
}

void rename_preserves_identity()
{
    using namespace arzoom;
    const PresentationScreenDiscoveredIdentity before{
        "uuid-stable", "Display Capture - Coding", true};
    const PresentationScreenDiscoveredIdentity after{
        "uuid-stable", "Renamed Coding Screen", true};
    require(presentation_screen_same_source_identity(before, after),
            "source rename changed durable Presentation Screen identity");
}

void scene_item_reorder_does_not_change_uuid_identity()
{
    using namespace arzoom;
    const std::array first_order{
        PresentationScreenDiscoveredIdentity{"uuid-a", "A", true},
        PresentationScreenDiscoveredIdentity{"uuid-b", "B", true},
    };
    const std::array second_order{
        PresentationScreenDiscoveredIdentity{"uuid-b", "B", true},
        PresentationScreenDiscoveredIdentity{"uuid-a", "A", true},
    };

    const auto before = presentation_screen_find_discovered_identity(
        first_order.data(), first_order.size(), "uuid-b");
    const auto after = presentation_screen_find_discovered_identity(
        second_order.data(), second_order.size(), "uuid-b");
    require(before.found() && after.found(),
            "UUID identity was lost after scene-item reorder");
    require(first_order[before.index].source_uuid ==
                second_order[after.index].source_uuid,
            "scene-item index accidentally became durable identity");
}

void missing_or_unresolved_uuid_fails_safe()
{
    using namespace arzoom;
    const std::array identities{
        PresentationScreenDiscoveredIdentity{"", "Missing UUID", true},
        PresentationScreenDiscoveredIdentity{"uuid-stale", "Stale", false},
    };
    require(!identities[0].valid() && !identities[1].valid(),
            "invalid discovered identities were accepted");

    const auto unresolved = presentation_screen_find_discovered_identity(
        identities.data(), identities.size(), "uuid-stale");
    require(unresolved.status == PresentationScreenIdentityLookupStatus::NotFound,
            "unresolved UUID was accepted as a durable identity");
}

void duplicate_uuid_is_ambiguous_never_guessed()
{
    using namespace arzoom;
    const std::array identities{
        PresentationScreenDiscoveredIdentity{"uuid-duplicate", "First", true},
        PresentationScreenDiscoveredIdentity{"uuid-duplicate", "Second", true},
    };
    const auto result = presentation_screen_find_discovered_identity(
        identities.data(), identities.size(), "uuid-duplicate");
    require(result.status == PresentationScreenIdentityLookupStatus::Ambiguous,
            "duplicate UUID identity did not fail ambiguous");
    require(!result.found(), "duplicate UUID guessed a candidate");
}

} // namespace

int main()
{
    two_candidates_resolve_independently_by_uuid();
    rename_preserves_identity();
    scene_item_reorder_does_not_change_uuid_identity();
    missing_or_unresolved_uuid_fails_safe();
    duplicate_uuid_is_ambiguous_never_guessed();
    std::cout << "ArZoom P4.2 UUID discovery identity gates: PASS\n";
    return 0;
}
