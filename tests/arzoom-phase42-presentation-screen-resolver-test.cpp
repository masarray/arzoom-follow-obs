#include "../src/arzoom-presentation-screen-resolver.hpp"

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

arzoom::PresentationScreenCandidate valid_screen(
    const char *uuid,
    arzoom::PresentationScreenRect monitor,
    bool eligible = true)
{
    arzoom::PresentationScreenCandidate candidate;
    candidate.source_uuid = uuid;
    candidate.physical_monitor = monitor;
    candidate.eligible = eligible;
    candidate.visible = true;
    candidate.source_resolved = true;
    candidate.monitor_resolved = true;
    candidate.geometry_valid = true;
    return candidate;
}

void zero_candidates_fail_safe()
{
    using namespace arzoom;
    const auto result = resolve_presentation_screen(nullptr, 0, 100, 100);
    require(result.status == PresentationScreenResolveStatus::NoEligibleScreens,
            "zero candidates did not report no eligible screens");
    require(!result.active(), "zero candidates unexpectedly produced an active mapping");
}

void cursor_inside_single_screen_selects_it()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 960, 540);
    require(result.status == PresentationScreenResolveStatus::Active,
            "single valid Presentation Screen did not activate");
    require(result.active_index == 0, "single valid Presentation Screen selected wrong index");
}

void two_screens_select_monitor_a()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {1920, 0, 3840, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 1000, 500);
    require(result.status == PresentationScreenResolveStatus::Active &&
                result.active_index == 0,
            "cursor on Monitor A did not select Mapping A");
}

void two_screens_select_monitor_b()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {1920, 0, 3840, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 2500, 500);
    require(result.status == PresentationScreenResolveStatus::Active &&
                result.active_index == 1,
            "cursor on Monitor B did not select Mapping B");
}

void cursor_outside_all_screens_never_guesses()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {1920, 0, 3840, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 5000, 500);
    require(result.status ==
                PresentationScreenResolveStatus::CursorOutsideEligibleScreens,
            "cursor outside Presentation Screens did not fail safe");
    require(!result.active(), "cursor outside Presentation Screens guessed an active mapping");
}

void overlapping_monitor_ownership_is_ambiguous()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {0, 0, 1920, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 700, 400);
    require(result.status ==
                PresentationScreenResolveStatus::AmbiguousMonitorOwnership,
            "duplicate physical-monitor ownership did not fail ambiguous");
    require(!result.active(), "ambiguous ownership guessed an active mapping");
}

void invalid_active_candidate_fails_safe()
{
    using namespace arzoom;

    auto hidden = valid_screen("uuid-a", {0, 0, 1920, 1080});
    hidden.visible = false;
    const std::array hidden_screens{hidden};
    auto result = resolve_presentation_screen(
        hidden_screens.data(), hidden_screens.size(), 500, 500);
    require(result.status == PresentationScreenResolveStatus::ActiveScreenInvalid,
            "hidden Presentation Screen under cursor did not fail safe");

    auto invalid_geometry = valid_screen("uuid-a", {0, 0, 1920, 1080});
    invalid_geometry.geometry_valid = false;
    const std::array geometry_screens{invalid_geometry};
    result = resolve_presentation_screen(
        geometry_screens.data(), geometry_screens.size(), 500, 500);
    require(result.status == PresentationScreenResolveStatus::ActiveScreenInvalid,
            "invalid geometry under cursor did not fail safe");

    auto invalid_mapping = valid_screen("uuid-a", {0, 0, 1920, 1080});
    invalid_mapping.mapping.scene_scale.x = 0.0f;
    const std::array mapping_screens{invalid_mapping};
    result = resolve_presentation_screen(
        mapping_screens.data(), mapping_screens.size(), 500, 500);
    require(result.status == PresentationScreenResolveStatus::ActiveScreenInvalid,
            "invalid mapping under cursor did not fail safe");

    auto unresolved_source = valid_screen("uuid-a", {0, 0, 1920, 1080});
    unresolved_source.source_resolved = false;
    const std::array unresolved_screens{unresolved_source};
    result = resolve_presentation_screen(
        unresolved_screens.data(), unresolved_screens.size(), 500, 500);
    require(result.status == PresentationScreenResolveStatus::ActiveScreenInvalid,
            "unresolved source UUID did not fail safe");

    auto missing_uuid = valid_screen("", {0, 0, 1920, 1080});
    const std::array missing_uuid_screens{missing_uuid};
    result = resolve_presentation_screen(
        missing_uuid_screens.data(), missing_uuid_screens.size(), 500, 500);
    require(result.status == PresentationScreenResolveStatus::ActiveScreenInvalid,
            "missing durable source UUID did not fail safe");
}

void negative_desktop_coordinates_are_supported()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-left", {-1920, -200, 0, 880}),
        valid_screen("uuid-primary", {0, 0, 1920, 1080}),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), -1200, 100);
    require(result.status == PresentationScreenResolveStatus::Active &&
                result.active_index == 0,
            "negative virtual-desktop coordinates did not select left monitor");
}

void excluded_utility_screen_never_activates()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-presentation", {0, 0, 1920, 1080}),
        valid_screen("uuid-utility", {1920, 0, 3840, 1080}, false),
    };
    const auto result = resolve_presentation_screen(
        screens.data(), screens.size(), 2500, 500);
    require(result.status ==
                PresentationScreenResolveStatus::CursorOutsideEligibleScreens,
            "excluded utility monitor participated in ownership");
    require(!result.active(), "excluded utility monitor became active");
}

void adjacent_boundary_is_deterministic_without_hysteresis()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {1920, 0, 3840, 1080}),
    };

    const auto left = resolve_presentation_screen(
        screens.data(), screens.size(), 1919, 500);
    const auto right = resolve_presentation_screen(
        screens.data(), screens.size(), 1920, 500);

    require(left.status == PresentationScreenResolveStatus::Active &&
                left.active_index == 0,
            "last pixel of Monitor A did not belong deterministically to A");
    require(right.status == PresentationScreenResolveStatus::Active &&
                right.active_index == 1,
            "first pixel of Monitor B did not belong deterministically to B");
}

void rapid_a_b_sequence_is_stateless_and_deterministic()
{
    using namespace arzoom;
    const std::array screens{
        valid_screen("uuid-a", {0, 0, 1920, 1080}),
        valid_screen("uuid-b", {1920, 0, 3840, 1080}),
    };
    constexpr std::array<std::int64_t, 8> cursor_x{
        100, 2500, 200, 3000, 400, 2200, 500, 2000};
    constexpr std::array<std::size_t, 8> expected{
        0, 1, 0, 1, 0, 1, 0, 1};

    for (std::size_t i = 0; i < cursor_x.size(); ++i) {
        const auto result = resolve_presentation_screen(
            screens.data(), screens.size(), cursor_x[i], 500);
        require(result.status == PresentationScreenResolveStatus::Active,
                "rapid A/B sequence produced a non-active transient state");
        require(result.active_index == expected[i],
                "rapid A/B sequence retained stale ownership state");
    }
}

} // namespace

int main()
{
    zero_candidates_fail_safe();
    cursor_inside_single_screen_selects_it();
    two_screens_select_monitor_a();
    two_screens_select_monitor_b();
    cursor_outside_all_screens_never_guesses();
    overlapping_monitor_ownership_is_ambiguous();
    invalid_active_candidate_fails_safe();
    negative_desktop_coordinates_are_supported();
    excluded_utility_screen_never_activates();
    adjacent_boundary_is_deterministic_without_hysteresis();
    rapid_a_b_sequence_is_stateless_and_deterministic();
    std::cout << "ArZoom Phase 4.2 PresentationScreenResolver gates: PASS\n";
    return 0;
}
