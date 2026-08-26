/* P4.2 Slice 6 integration gate: one owner, four consumers. */
#include "arzoom-click-visual.hpp"
#include "arzoom-presentation-screen-active-mapping.hpp"
#include "arzoom-scene-mapping-runtime.hpp"
#include "arzoom-smart-zone-camera.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifndef ARZOOM_SOURCE_ROOT
#error ARZOOM_SOURCE_ROOT must point at the repository root
#endif

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool near(float a, float b, float epsilon = 0.0005f)
{
    return std::fabs(a - b) <= epsilon;
}

bool near(arzoom::Vec2 a, arzoom::Vec2 b, float epsilon = 0.0005f)
{
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

std::string read_source(const char *relative_path)
{
    const std::string path =
        std::string(ARZOOM_SOURCE_ROOT) + "/" + relative_path;
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "could not read source contract file: " + path);

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string compact_source(std::string text)
{
    text.erase(
        std::remove_if(
            text.begin(), text.end(),
            [](unsigned char c) { return std::isspace(c) != 0; }),
        text.end());
    return text;
}

void require_contains(const std::string &text, const std::string &needle,
                      const char *message)
{
    require(text.find(needle) != std::string::npos, message);
}

void require_not_contains(const std::string &text, const std::string &needle,
                          const char *message)
{
    require(text.find(needle) == std::string::npos, message);
}

std::size_t occurrence_count(const std::string &text,
                             const std::string &needle)
{
    if (needle.empty())
        return 0;

    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

arzoom::SceneAxisAlignedMapping mapping(float offset_x, float scale_x)
{
    arzoom::SceneAxisAlignedMapping result;
    result.source_visible_min = {0.0f, 0.0f};
    result.source_visible_max = {1.0f, 1.0f};
    result.scene_offset = {offset_x, 0.0f};
    result.scene_scale = {scale_x, 1.0f};
    require(result.valid(), "test mapping must be valid");
    return result;
}

arzoom::PresentationScreenActiveMappingCandidate candidate(
    const char *uuid, const char *label,
    arzoom::PresentationScreenRect physical_monitor,
    arzoom::SceneAxisAlignedMapping scene_mapping)
{
    arzoom::PresentationScreenActiveMappingCandidate result;
    result.identity.source_uuid = uuid ? uuid : "";
    result.identity.display_label = label ? label : "";
    result.identity.source_resolved = uuid && *uuid;
    result.physical_monitor = physical_monitor;
    result.mapping = scene_mapping;
    result.visible = true;
    result.monitor_resolved = true;
    result.geometry_valid = true;
    result.runtime_ready = true;
    return result;
}

arzoom::PresentationScreenSelectionSettings select_both()
{
    arzoom::PresentationScreenSelectionSettings settings;
    settings.persisted = true;
    settings.selected_source_uuids = {"uuid-a", "uuid-b"};
    return settings;
}

arzoom::Vec2 normalize(const arzoom::SceneDesktopRect &rect,
                       std::int64_t x, std::int64_t y)
{
    require(rect.valid(), "desktop rect must be valid before normalization");
    return {
        static_cast<float>(x - rect.left) /
            static_cast<float>(rect.right - rect.left),
        static_cast<float>(y - rect.top) /
            static_cast<float>(rect.bottom - rect.top),
    };
}

arzoom::Vec2 shared_scene_content(
    const arzoom::PresentationScreenActiveMappingSet &prepared,
    const arzoom::PresentationScreenResolveResult &resolved,
    std::int64_t cursor_x, std::int64_t cursor_y)
{
    require(resolved.active(), "shared consumer trace requires active mapping");
    require(resolved.active_index < prepared.resolver_candidates.size(),
            "resolved index must reference prepared candidate");

    const auto &active = prepared.resolver_candidates[resolved.active_index];
    const arzoom::SceneDesktopRect physical{
        active.physical_monitor.left,
        active.physical_monitor.top,
        active.physical_monitor.right,
        active.physical_monitor.bottom,
    };

    arzoom::SceneDesktopRect mapped;
    require(arzoom::scene_mapping_build_synthetic_desktop_rect(
                physical, active.mapping,
                static_cast<std::int64_t>(
                    std::numeric_limits<std::int32_t>::lowest()),
                static_cast<std::int64_t>(
                    std::numeric_limits<std::int32_t>::max()),
                mapped),
            "active mapping must build the inherited synthetic monitor");

    const arzoom::Vec2 source_uv = normalize(physical, cursor_x, cursor_y);
    const arzoom::Vec2 expected_scene =
        arzoom::scene_mapping_source_to_scene(active.mapping, source_uv);
    const arzoom::Vec2 inherited_scene =
        normalize(mapped, cursor_x, cursor_y);

    require(near(inherited_scene, expected_scene, 0.0025f),
            "synthetic monitor normalization must equal proven scene mapping");
    return inherited_scene;
}

void one_owner_four_consumers_trace()
{
    const std::vector<arzoom::PresentationScreenActiveMappingCandidate>
        candidates = {
            candidate(
                "uuid-a", "Coding",
                {0, 0, 1920, 1080},
                mapping(0.0f, 0.5f)),
            candidate(
                "uuid-b", "Application",
                {1920, 0, 3840, 1080},
                mapping(0.5f, 0.5f)),
        };

    const auto prepared = arzoom::presentation_screen_prepare_active_mapping(
        candidates.data(), candidates.size(), select_both());
    require(prepared.ready(),
            "two explicitly selected screens should prepare one resolver set");

    const auto resolved_a =
        arzoom::presentation_screen_resolve_active_mapping(
            prepared, 960, 540);
    const auto resolved_b =
        arzoom::presentation_screen_resolve_active_mapping(
            prepared, 2880, 540);

    require(resolved_a.active() && resolved_a.active_index == 0,
            "cursor on Monitor A must select mapping A");
    require(resolved_b.active() && resolved_b.active_index == 1,
            "cursor on Monitor B must select mapping B");

    const arzoom::Vec2 content_a =
        shared_scene_content(prepared, resolved_a, 960, 540);
    const arzoom::Vec2 content_b =
        shared_scene_content(prepared, resolved_b, 2880, 540);

    require(near(content_a, {0.25f, 0.50f}, 0.0025f),
            "Monitor A center must map to left scene half");
    require(near(content_b, {0.75f, 0.50f}, 0.0025f),
            "Monitor B center must map to right scene half");

    for (const arzoom::Vec2 shared_content : {content_a, content_b}) {
        arzoom::CameraInput smart_follow_input;
        smart_follow_input.cursor = shared_content;
        smart_follow_input.cursor_valid = true;

        arzoom::ClickVisualState clicks;
        clicks.push(arzoom::ClickType::Left, shared_content);
        require(clicks.slot(0).active(),
                "click consumer must retain the shared mapping input");

        const arzoom::Vec2 presentation_cursor = shared_content;
        const arzoom::Vec2 spotlight_input = shared_content;

        require(near(smart_follow_input.cursor,
                     clicks.slot(0).content_position),
                "Smart Follow and click must receive the same content mapping");
        require(near(smart_follow_input.cursor, presentation_cursor),
                "Smart Follow and Presentation Cursor must receive the same mapping");
        require(near(smart_follow_input.cursor, spotlight_input),
                "Smart Follow and Spotlight must receive the same mapping");

        const arzoom::Vec2 camera_center{0.5f, 0.5f};
        const float zoom = 2.0f;
        const arzoom::Vec2 click_output =
            arzoom::project_content_to_output(
                clicks.slot(0).content_position, camera_center, zoom);
        const arzoom::Vec2 spotlight_output =
            arzoom::project_content_to_output(
                spotlight_input, camera_center, zoom);
        require(near(click_output, spotlight_output),
                "click and Spotlight projection must stay spatially identical");
    }
}

void runtime_source_contract_has_one_owner()
{
    const std::string active = compact_source(
        read_source("src/arzoom-filter-p42-active.cpp"));
    const std::string phase1 = compact_source(
        read_source("src/arzoom-filter-v2.cpp"));
    const std::string click = compact_source(
        read_source("src/arzoom-filter-v3.cpp"));
    const std::string presenter = compact_source(
        read_source("src/arzoom-filter-v4.cpp"));
    const std::string spotlight = compact_source(
        read_source("src/arzoom-filter-v15.cpp"));
    const std::string spotlight_runtime = compact_source(
        read_source("src/arzoom-filter-v16.cpp"));
    const std::string cinematic = compact_source(
        read_source("src/arzoom-filter-v23.cpp"));

    require(
        occurrence_count(active,
                         "presentation_screen_resolve_active_mapping(") == 1,
        "active adapter must be the single runtime Presentation Screen resolver");
    require_contains(
        active,
        "phase41->mapped_monitor=candidate.mapped_monitor;",
        "active adapter must publish exactly one synthetic mapped monitor");

    require_contains(
        phase1,
        "input.cursor=cursor_normalized(filter->monitor,cursor_x,cursor_y);",
        "Smart Follow must normalize from phase1->monitor");
    require_contains(
        click,
        "constarzoom::Vec2position=cursor_normalized(filter->phase1->monitor,cursor_x,cursor_y);",
        "click anchor must normalize from phase1->monitor");
    require_contains(
        presenter,
        "cursor=cursor_normalized(phase1->monitor,cursor_x,cursor_y);",
        "Presentation Cursor must normalize from phase1->monitor");
    require_contains(
        spotlight,
        "content_position=cursor_normalized(phase1->monitor,cursor_x,cursor_y);",
        "Spotlight cursor mapping must normalize from phase1->monitor");
    require_contains(
        spotlight_runtime,
        "if(!phase1||!mapped_pointer_content(phase1,content))returnfalse;",
        "Spotlight Cursor/Smart Focus must consume mapped_pointer_content");
    require_contains(
        spotlight_runtime,
        "filter->click_anchor_content=newest->content_position;",
        "Spotlight Click must consume the Phase 2 click content anchor");
    require_contains(
        cinematic,
        "if(!phase51_pointer_output(filter,pointer_output))returnfilter->center_valid;",
        "cinematic Spotlight auto center must consume the shared pointer path");

    for (const auto *consumer_source :
         {&phase1, &click, &presenter, &spotlight,
          &spotlight_runtime, &cinematic}) {
        require_not_contains(
            *consumer_source,
            "presentation_screen_resolve_active_mapping(",
            "downstream consumer must not resolve Presentation Screens independently");
        require_not_contains(
            *consumer_source,
            "discover_scene_display_candidates(",
            "downstream consumer must not discover Display Captures independently");
        require_not_contains(
            *consumer_source,
            "obs_get_source_by_uuid(",
            "downstream consumer must not perform UUID ownership resolution");
    }
}

} // namespace

int main()
{
    one_owner_four_consumers_trace();
    runtime_source_contract_has_one_owner();
    std::cout
        << "ArZoom Phase 4.2 shared active mapping consumer consistency: PASS\n";
    return 0;
}
