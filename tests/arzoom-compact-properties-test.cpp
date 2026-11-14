#include "../src/arzoom-properties-ux-policy.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using arzoom::CompactPropertiesPolicy;

    const CompactPropertiesPolicy novice{};
    assert(!novice.show_camera_advanced());
    assert(!novice.show_spotlight_primary());
    assert(!novice.show_spotlight_advanced());
    assert(!novice.show_cinematic_speed());

    const CompactPropertiesPolicy spotlight_on{false, true, true};
    assert(spotlight_on.show_spotlight_primary());
    assert(!spotlight_on.show_spotlight_advanced());
    assert(!spotlight_on.show_cinematic_speed());

    const CompactPropertiesPolicy advanced_only{true, false, true};
    assert(advanced_only.show_camera_advanced());
    assert(!advanced_only.show_spotlight_primary());
    assert(!advanced_only.show_spotlight_advanced());
    assert(!advanced_only.show_cinematic_speed());

    const CompactPropertiesPolicy custom{true, true, false};
    assert(custom.show_camera_advanced());
    assert(custom.show_spotlight_primary());
    assert(custom.show_spotlight_advanced());
    assert(!custom.show_cinematic_speed());

    const CompactPropertiesPolicy cinematic{true, true, true};
    assert(cinematic.show_cinematic_speed());

    std::cout << "ArZoom compact OBS UX policy: PASS\n";
    return 0;
}
