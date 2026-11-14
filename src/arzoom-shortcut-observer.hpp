#pragma once
#include "arzoom-shortcut.hpp"

#include <cstdint>

namespace arzoom::shortcut {

/* Win32 input is deliberately isolated behind this tiny semantic snapshot.
 * No key history, text strings, scan codes or app/window references escape
 * the observer. One optional hook per OBS module, never one per filter. */
struct Snapshot {
    presentation::ShortcutChord chord{};
    std::uint32_t generation = 0;
};

/* Nonblocking on the OBS video tick; returns false if its worker could not start. */
bool subscribe();
void unsubscribe();
Snapshot snapshot() noexcept;
void shutdown();

} // namespace arzoom::shortcut
