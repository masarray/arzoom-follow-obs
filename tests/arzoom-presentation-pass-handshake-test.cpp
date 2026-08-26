#include "arzoom-presentation-pass-handshake.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using namespace arzoom;

    assert(kPresentationWarmFrameCount == 3);
    assert(merge_presentation_warm_frames(0) == 3);
    assert(merge_presentation_warm_frames(2) == 3);
    assert(merge_presentation_warm_frames(5) == 5);
    assert(merge_presentation_warm_frames(-1) == 3);
    assert(merge_presentation_warm_frames(1, 2) == 2);

    assert(!presentation_pass_activation_edge(false, false));
    assert(presentation_pass_activation_edge(false, true));
    assert(!presentation_pass_activation_edge(true, true));
    assert(!presentation_pass_activation_edge(true, false));

    int pending = merge_presentation_warm_frames(0);
    assert(pending == 3);
    pending = consume_presentation_warm_frame(pending);
    assert(pending == 2);
    pending = consume_presentation_warm_frame(pending);
    assert(pending == 1);
    pending = consume_presentation_warm_frame(pending);
    assert(pending == 0);
    pending = consume_presentation_warm_frame(pending);
    assert(pending == 0);

    /* Repeated update/resource requests remain bounded instead of accumulating. */
    pending = merge_presentation_warm_frames(1);
    pending = merge_presentation_warm_frames(pending);
    pending = merge_presentation_warm_frames(pending);
    assert(pending == kPresentationWarmFrameCount);

    std::cout << "Presentation-pass activation handshake invariants passed\n";
    return 0;
}
