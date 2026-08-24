#include "arzoom-filter-v6.cpp"

/*
 * Keep OBS property callbacks type-safe after the P3.5 trial fix wraps the
 * Phase35Filter in Phase351Filter. All runtime/render callbacks are already
 * overridden by v6; this bridge only forwards properties to the underlying
 * Phase35Filter instead of letting OBS reinterpret the wrapper pointer.
 */
namespace {

obs_properties_t *phase351_properties(void *data)
{
    auto *filter = static_cast<Phase351Filter *>(data);
    return phase35_properties(filter ? filter->phase35 : nullptr);
}

struct Phase351PropertiesOverride {
    Phase351PropertiesOverride()
    {
        arzoom_filter_info.get_properties = phase351_properties;
    }
};

Phase351PropertiesOverride phase351_properties_override;

} // namespace
