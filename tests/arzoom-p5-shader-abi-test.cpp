#include "arzoom-spotlight-abi.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool finite(float value)
{
    return std::isfinite(value);
}

} // namespace

int main()
{
    const auto abi = arzoom::spotlight_neutral_abi();

    assert(finite(abi.center_x) && finite(abi.center_y));
    assert(finite(abi.half_size_x_px) && finite(abi.half_size_y_px));
    assert(finite(abi.feather_px));
    assert(finite(abi.dim_strength));
    assert(finite(abi.shape));
    assert(finite(abi.corner_radius_px));
    assert(finite(abi.area_scale));
    assert(finite(abi.circle));
    assert(finite(abi.enabled));

    /* Neutral packet must be visually inert but mathematically valid for every
     * shader expression even on backends that validate all technique params. */
    assert(abi.enabled == 0.0f);
    assert(abi.dim_strength == 0.0f);
    assert(abi.center_x >= 0.0f && abi.center_x <= 1.0f);
    assert(abi.center_y >= 0.0f && abi.center_y <= 1.0f);
    assert(abi.half_size_x_px >= 1.0f);
    assert(abi.half_size_y_px >= 1.0f);
    assert(abi.feather_px >= 1.0f);
    assert(abi.corner_radius_px >= 0.0f);
    assert(abi.area_scale >= 0.5f && abi.area_scale <= 2.0f);
    assert(abi.circle == 1.0f);

    std::cout << "P5 neutral shader ABI contract passed\n";
    return 0;
}
