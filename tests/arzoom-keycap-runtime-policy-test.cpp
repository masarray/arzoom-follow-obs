#include "../src/arzoom-keycap-runtime-policy.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using arzoom::shortcut::ObserverActivation;
    for (int flags = 0; flags < 8; ++flags) {
        const ObserverActivation p{(flags & 1) != 0,
                                   (flags & 2) != 0,
                                   (flags & 4) != 0};
        assert(p.subscribe() == (flags == 7));
    }

    assert(arzoom::shortcut::use_legacy_presentation(false));
    assert(!arzoom::shortcut::use_legacy_presentation(true));

    std::cout << "ArZoom keycap observer privacy/fallback policy: PASS\n";
    return 0;
}
