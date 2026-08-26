#pragma once

#include <algorithm>

namespace arzoom {

/*
 * A shared OBS effect may spend long periods in true pass-through and then be
 * activated by the first click, a Presentation Cursor resource replacement, or
 * another presentation feature. Direct OBS 32.2.2 testing showed that jumping
 * straight from skip/pass-through into a resource-bearing frame can leave the
 * filtered source black until another processed frame is forced.
 *
 * Keep the recovery contract tiny and deterministic. A transition requests a
 * small fixed number of neutral processed frames. Requests merge by max(), not
 * addition, so rapid GUI/resource changes can never grow unbounded work.
 */
constexpr int kPresentationWarmFrameCount = 3;

inline int merge_presentation_warm_frames(
    int current_pending,
    int requested = kPresentationWarmFrameCount)
{
    return std::max(std::max(current_pending, 0),
                    std::max(requested, 0));
}

inline bool presentation_pass_activation_edge(bool previously_required,
                                              bool currently_required)
{
    return currently_required && !previously_required;
}

inline int consume_presentation_warm_frame(int pending)
{
    return pending > 0 ? pending - 1 : 0;
}

} // namespace arzoom
