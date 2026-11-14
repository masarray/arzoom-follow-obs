#include "../src/arzoom-shortcut-timeline.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char *why)
{
    if (!condition) {
        std::cerr << "FAIL: " << why << '\n';
        std::abort();
    }
}

void presentation_timeline()
{
    using namespace arzoom;
    using namespace arzoom::shortcut;
    Timeline timeline;
    require(!timeline.active() && timeline.opacity() == 0.0f,
            "new timeline is not transparent");

    const Snapshot save{{presentation::ShortcutKey::S,
                          presentation::ShortcutCtrl}, 1};
    timeline.step(save, 1.0f / 60.0f);
    require(timeline.active() && timeline.frame().keycap_count == 2,
            "Ctrl+S not visible as a two-key semantic chord");
    require(timeline.opacity() == 0.0f,
            "new chord must fade in, not jump from transparent");
    timeline.step(save, 0.035f);
    require(timeline.opacity() > 0.4f && timeline.opacity() < 0.6f,
            "70 ms fade-in drifted");
    timeline.step(save, 0.035f);
    require(timeline.opacity() == 1.0f, "fade-in not completed");
    for (int i = 0; i < 72; ++i)
        timeline.step(save, 1.0f / 60.0f);
    require(timeline.active(), "hold ended prematurely");
    for (int i = 0; i < 17; ++i)
        timeline.step(save, 1.0f / 60.0f);
    require(!timeline.active() && timeline.opacity() == 0.0f,
            "keycap did not fade out to full transparency");

    const Snapshot copy{{presentation::ShortcutKey::C,
                          presentation::ShortcutCtrl}, 2};
    timeline.step(copy, 1.0f / 60.0f);
    require(timeline.active() && timeline.frame().generation == 2,
            "next semantic chord did not replace previous overlay");
    timeline.reset();
    require(!timeline.active(), "disable/deactivate retained keycaps");
}

void invariant_against_plain_typing()
{
    using namespace arzoom;
    using namespace arzoom::shortcut;
    Timeline timeline;
    const Snapshot typing{{presentation::ShortcutKey::A, 0}, 42};
    timeline.step(typing, 0.05f);
    require(!timeline.active(),
            "ordinary unmodified typing produced a keycap");
    timeline.step({{presentation::ShortcutKey::C, 0x80}, 43}, 0.05f);
    require(!timeline.active(),
            "invalid-modifier typing produced a keycap");
    timeline.step({{presentation::ShortcutKey::F12, 0}, 44}, 0.05f);
    require(timeline.active(), "F12 action was incorrectly suppressed");
}

void seconds_not_frames()
{
    using namespace arzoom;
    using namespace arzoom::shortcut;
    for (int fps : {30, 60, 120, 144}) {
        Timeline timeline;
        const Snapshot chord{{presentation::ShortcutKey::P,
                              presentation::ShortcutCtrl}, 1};
        timeline.step(chord, 0.0f);
        for (int i = 0; i < fps; ++i)
            timeline.step(chord, 1.0f / fps);
        require(timeline.active(),
                "1 second hold is not frame-rate independent");
        for (int i = 0; i < fps; ++i)
            timeline.step(chord, 1.0f / fps);
        require(!timeline.active(),
                "expired keycap persists after 2 seconds");
    }
}
} // namespace

int main()
{
    presentation_timeline();
    invariant_against_plain_typing();
    seconds_not_frames();
    std::cout << "ArZoom shortcut timeline gates: PASS\n";
}
