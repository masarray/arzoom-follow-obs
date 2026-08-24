#include "../src/arzoom-camera-source-core.hpp"

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

void render_target_policy_is_bounded_and_deterministic()
{
    using namespace arzoom;

    require(camera_render_target_action({0, 0}, {1920, 1080}) ==
                CameraRenderTargetAction::Recreate,
            "first valid target did not request allocation");
    require(camera_render_target_action({1920, 1080}, {1920, 1080}) ==
                CameraRenderTargetAction::Reuse,
            "unchanged target dimensions did not reuse render target");
    require(camera_render_target_action({1920, 1080}, {2560, 1440}) ==
                CameraRenderTargetAction::Recreate,
            "size change did not request exactly one recreate decision");

    for (CameraRenderSize invalid : std::array<CameraRenderSize, 5>{
             CameraRenderSize{0, 1080}, CameraRenderSize{1920, 0},
             CameraRenderSize{0, 0}, CameraRenderSize{32769, 1080},
             CameraRenderSize{1920, 32769}}) {
        require(camera_render_target_action({1920, 1080}, invalid) ==
                    CameraRenderTargetAction::Invalid,
                "invalid dimensions were accepted");
    }
}

void recursion_guard_blocks_self_and_indirect_cycles()
{
    using arzoom::CameraRenderRecursionGuard;

    int camera_a = 0;
    int camera_b = 0;
    int camera_c = 0;

    require(CameraRenderRecursionGuard::active_depth_for_test() == 0,
            "recursion stack was not initially empty");

    {
        CameraRenderRecursionGuard a(&camera_a);
        require(a.entered(), "first camera render was rejected");
        require(CameraRenderRecursionGuard::active_depth_for_test() == 1,
                "first camera render did not enter stack");

        CameraRenderRecursionGuard self(&camera_a);
        require(!self.entered(), "direct self recursion was not blocked");
        require(CameraRenderRecursionGuard::active_depth_for_test() == 1,
                "rejected self recursion changed stack depth");

        CameraRenderRecursionGuard b(&camera_b);
        require(b.entered(), "second distinct camera could not nest");

        CameraRenderRecursionGuard c(&camera_c);
        require(c.entered(), "third distinct camera could not nest");

        CameraRenderRecursionGuard indirect(&camera_a);
        require(!indirect.entered(),
                "indirect A -> B -> C -> A recursion was not blocked");
        require(CameraRenderRecursionGuard::active_depth_for_test() == 3,
                "rejected indirect recursion changed stack depth");
    }

    require(CameraRenderRecursionGuard::active_depth_for_test() == 0,
            "recursion stack did not unwind after render scope");

    CameraRenderRecursionGuard again(&camera_a);
    require(again.entered(), "camera stayed poisoned after prior render scope");
}

void recursion_guard_has_hard_depth_limit()
{
    using arzoom::CameraRenderRecursionGuard;

    std::array<int, CameraRenderRecursionGuard::kMaxDepth + 1> tokens{};
    std::array<CameraRenderRecursionGuard *, CameraRenderRecursionGuard::kMaxDepth>
        guards{};

    for (size_t i = 0; i < CameraRenderRecursionGuard::kMaxDepth; ++i) {
        guards[i] = new CameraRenderRecursionGuard(&tokens[i]);
        require(guards[i]->entered(), "valid nesting hit depth limit too early");
    }

    require(CameraRenderRecursionGuard::active_depth_for_test() ==
                CameraRenderRecursionGuard::kMaxDepth,
            "recursion guard depth accounting mismatch");

    CameraRenderRecursionGuard overflow(&tokens.back());
    require(!overflow.entered(), "recursion guard allowed unbounded nesting");

    for (size_t i = CameraRenderRecursionGuard::kMaxDepth; i > 0; --i)
        delete guards[i - 1];

    require(CameraRenderRecursionGuard::active_depth_for_test() == 0,
            "bounded recursion stack did not fully unwind");
}

} // namespace

int main()
{
    render_target_policy_is_bounded_and_deterministic();
    recursion_guard_blocks_self_and_indirect_cycles();
    recursion_guard_has_hard_depth_limit();
    std::cout << "ArZoom Phase 4 Camera source safety gates: PASS\n";
    return 0;
}
