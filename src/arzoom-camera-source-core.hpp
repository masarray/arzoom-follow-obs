#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arzoom {

struct CameraRenderSize {
    uint32_t width = 0;
    uint32_t height = 0;
};

inline bool camera_render_size_valid(CameraRenderSize size)
{
    return size.width > 0 && size.height > 0 &&
           size.width <= 32768 && size.height <= 32768;
}

enum class CameraRenderTargetAction {
    Invalid,
    Reuse,
    Recreate,
};

inline CameraRenderTargetAction camera_render_target_action(
    CameraRenderSize current, CameraRenderSize requested)
{
    if (!camera_render_size_valid(requested))
        return CameraRenderTargetAction::Invalid;
    if (camera_render_size_valid(current) &&
        current.width == requested.width &&
        current.height == requested.height)
        return CameraRenderTargetAction::Reuse;
    return CameraRenderTargetAction::Recreate;
}

/*
 * Render recursion protection for ArZoom Camera.
 *
 * A selected scene may accidentally contain the same ArZoom Camera, or two
 * camera sources may reference scenes that lead back to each other. The guard
 * uses a bounded thread-local render stack. Re-entering an already-active
 * token is rejected immediately, as is exceeding the hard nesting limit.
 *
 * This state is render-thread-local, allocation-free, and intentionally has no
 * OBS dependency so the contract can be CI-tested on every platform.
 */
class CameraRenderRecursionGuard {
public:
    static constexpr size_t kMaxDepth = 16;

    explicit CameraRenderRecursionGuard(const void *token) noexcept
        : token_(token)
    {
        if (!token_ || depth_ >= kMaxDepth)
            return;
        for (size_t i = 0; i < depth_; ++i) {
            if (stack_[i] == token_)
                return;
        }
        stack_[depth_++] = token_;
        entered_ = true;
    }

    CameraRenderRecursionGuard(const CameraRenderRecursionGuard &) = delete;
    CameraRenderRecursionGuard &operator=(const CameraRenderRecursionGuard &) = delete;

    ~CameraRenderRecursionGuard()
    {
        if (!entered_)
            return;
        if (depth_ > 0 && stack_[depth_ - 1] == token_) {
            stack_[--depth_] = nullptr;
            return;
        }

        /* Defensive recovery: this should never occur with normal RAII/LIFO
         * usage, but do not leave a poisoned stack if a future caller breaks
         * that assumption. */
        for (size_t i = depth_; i > 0; --i) {
            if (stack_[i - 1] == token_) {
                for (size_t j = i; j < depth_; ++j)
                    stack_[j - 1] = stack_[j];
                stack_[--depth_] = nullptr;
                break;
            }
        }
    }

    bool entered() const noexcept { return entered_; }
    explicit operator bool() const noexcept { return entered_; }

    static size_t active_depth_for_test() noexcept { return depth_; }

private:
    const void *token_ = nullptr;
    bool entered_ = false;

    inline static thread_local std::array<const void *, kMaxDepth> stack_{};
    inline static thread_local size_t depth_ = 0;
};

} // namespace arzoom
