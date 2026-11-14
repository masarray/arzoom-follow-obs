#pragma once

namespace arzoom::shortcut {

/* Pure ownership contract for the optional process-global keyboard observer.
 * The plugin's OBS runtime supplies the 'showing' flag; this policy cannot
 * install hooks, read keys or change any camera state on its own. */
struct ObserverActivation {
    bool opted_in = false;
    bool filter_enabled = false;
    bool showing = false;

    constexpr bool subscribe() const noexcept
    {
        return opted_in && filter_enabled && showing;
    }
};

/* Legacy effect payloads do not contain the optional keycap ABI.
 * Preserve inherited camera/Click rendering instead of turning the entire
 * OBS source black. A declared-but-unbound sampler still fails safely. */
constexpr bool use_legacy_presentation(bool keycap_abi_ready) noexcept
{
    return !keycap_abi_ready;
}

} // namespace arzoom::shortcut
