#include "arzoom-shortcut-observer.hpp"
#include "arzoom-shortcut-visualizer.hpp"
#include <obs-module.h>

#include <atomic>
#include <cstdint>

#ifdef _WIN32
/* The plugin CMake target already defines WIN32_LEAN_AND_MEAN/NOMINMAX;
 * do not redeclare them under MSVC /WX (C4005 -> C2220). */
#include <windows.h>

#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <system_error>

namespace arzoom::shortcut {
namespace {

constexpr UINT kObserverWake = WM_APP + 0x47;
constexpr unsigned kVirtualKeyCapacity = 256;
std::atomic<std::uint64_t> published{0};
std::atomic<bool> opt_in_active{false};
std::atomic<DWORD> hook_thread_id{0};
std::mutex lifecycle_mutex;
std::condition_variable ready_cv;
std::thread worker;
unsigned user_count = 0;
bool message_queue_ready = false;

/* Hook-thread-owned only. Zero contention and no callback allocations. */
std::array<bool, kVirtualKeyCapacity> keys_down{};
std::uint32_t sequence = 0;
bool right_alt_down = false;

presentation::ShortcutKey canonical_key(DWORD code) noexcept
{
    using namespace presentation;
    if ((code >= '0' && code <= '9') ||
        (code >= 'A' && code <= 'Z'))
        return static_cast<ShortcutKey>(code);
    if (code >= VK_F1 && code <= VK_F24)
        return static_cast<ShortcutKey>(
            static_cast<unsigned>(ShortcutKey::F1) + code - VK_F1);
    switch (code) {
    case VK_ESCAPE: return ShortcutKey::Escape;
    case VK_TAB: return ShortcutKey::Tab;
    case VK_RETURN: return ShortcutKey::Enter;
    case VK_SPACE: return ShortcutKey::Space;
    case VK_BACK: return ShortcutKey::Backspace;
    case VK_DELETE: return ShortcutKey::Delete;
    case VK_INSERT: return ShortcutKey::Insert;
    case VK_HOME: return ShortcutKey::Home;
    case VK_END: return ShortcutKey::End;
    case VK_PRIOR: return ShortcutKey::PageUp;
    case VK_NEXT: return ShortcutKey::PageDown;
    case VK_LEFT: return ShortcutKey::Left;
    case VK_RIGHT: return ShortcutKey::Right;
    case VK_UP: return ShortcutKey::Up;
    case VK_DOWN: return ShortcutKey::Down;
    case VK_SNAPSHOT: return ShortcutKey::PrintScreen;
    case VK_PAUSE: return ShortcutKey::Pause;
    case VK_APPS: return ShortcutKey::Apps;
    case VK_OEM_PLUS: return ShortcutKey::Plus;
    case VK_OEM_MINUS: return ShortcutKey::Minus;
    case VK_OEM_COMMA: return ShortcutKey::Comma;
    case VK_OEM_PERIOD: return ShortcutKey::Period;
    case VK_OEM_2: return ShortcutKey::Slash;
    case VK_OEM_5: return ShortcutKey::Backslash;
    case VK_OEM_1: return ShortcutKey::Semicolon;
    case VK_OEM_7: return ShortcutKey::Quote;
    case VK_OEM_4: return ShortcutKey::LeftBracket;
    case VK_OEM_6: return ShortcutKey::RightBracket;
    case VK_OEM_3: return ShortcutKey::Grave;
    case VK_VOLUME_MUTE: return ShortcutKey::VolumeMute;
    case VK_VOLUME_DOWN: return ShortcutKey::VolumeDown;
    case VK_VOLUME_UP: return ShortcutKey::VolumeUp;
    case VK_MEDIA_NEXT_TRACK: return ShortcutKey::MediaNext;
    case VK_MEDIA_PREV_TRACK: return ShortcutKey::MediaPrevious;
    case VK_MEDIA_STOP: return ShortcutKey::MediaStop;
    case VK_MEDIA_PLAY_PAUSE: return ShortcutKey::MediaPlayPause;
    default: return ShortcutKey::Unknown;
    }
}

bool down(int normal, int left, int right) noexcept
{
    return keys_down[normal] || keys_down[left] || keys_down[right];
}

std::uint8_t active_modifiers() noexcept
{
    using namespace presentation;
    std::uint8_t mods = 0;
    if (down(VK_CONTROL, VK_LCONTROL, VK_RCONTROL)) mods |= ShortcutCtrl;
    if (down(VK_SHIFT, VK_LSHIFT, VK_RSHIFT)) mods |= ShortcutShift;
    if (down(VK_MENU, VK_LMENU, VK_RMENU)) mods |= ShortcutAlt;
    if (keys_down[VK_LWIN] || keys_down[VK_RWIN]) mods |= ShortcutWin;
    return mods;
}

bool is_modifier(DWORD code) noexcept
{
    switch (code) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN: return true;
    default: return false;
    }
}

LRESULT CALLBACK shortcut_hook(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && opt_in_active.load(std::memory_order_relaxed)) {
        const auto *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
        const DWORD vk = key->vkCode;
        if (vk < kVirtualKeyCapacity &&
            (key->flags & LLKHF_INJECTED) == 0) {
            const bool pressed =
                message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            const bool released =
                message == WM_KEYUP || message == WM_SYSKEYUP;
            if (pressed || released) {
                /* Right Alt often enters international text (AltGr), even
                 * when Windows synthesizes/marks Ctrl inconsistently. */
                if (vk == VK_RMENU ||
                    (vk == VK_MENU &&
                     (key->flags & LLKHF_EXTENDED) != 0))
                    right_alt_down = pressed;
                const bool duplicate = pressed && keys_down[vk];
                keys_down[vk] = pressed;
                if (pressed && !duplicate && !is_modifier(vk)) {
                    const presentation::ShortcutChord chord{
                        canonical_key(vk), active_modifiers()};
                    using namespace presentation;
                    const bool shift_typing =
                        chord.modifiers == ShortcutShift &&
                        is_textual_shortcut_key(chord.key);
                    const bool altgr_typing =
                        (chord.modifiers & (ShortcutCtrl | ShortcutAlt)) ==
                            (ShortcutCtrl | ShortcutAlt) &&
                        (chord.modifiers & ShortcutWin) == 0 &&
                        is_textual_shortcut_key(chord.key);
                    if (!shift_typing && !altgr_typing &&
                        !(right_alt_down &&
                          is_textual_shortcut_key(chord.key)) &&
                        should_visualize_shortcut(chord)) {
                        ++sequence;
                        if (sequence == 0) ++sequence;
                        published.store(
                            (static_cast<std::uint64_t>(sequence) << 32) |
                            (static_cast<std::uint64_t>(chord.modifiers) << 16) |
                            static_cast<std::uint16_t>(chord.key),
                            std::memory_order_release);
                    }
                }
            }
        }
    }
    /* OBS and every other app always receive the original keyboard event. */
    return CallNextHookEx(nullptr, code, message, data);
}

void observer_thread()
{
    MSG msg{};
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex);
        hook_thread_id.store(GetCurrentThreadId(), std::memory_order_release);
        message_queue_ready = true;
    }
    ready_cv.notify_all();

    HHOOK hook = nullptr;
    auto sync_hook = [&]() {
        const bool need = opt_in_active.load(std::memory_order_acquire);
        if (!need && hook) {
            UnhookWindowsHookEx(hook);
            hook = nullptr;
            keys_down.fill(false);
            right_alt_down = false;
        } else if (need && !hook) {
            HMODULE module = nullptr;
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&shortcut_hook), &module);
            hook = SetWindowsHookExW(
                WH_KEYBOARD_LL, shortcut_hook, module, 0);
            if (!hook)
                blog(LOG_WARNING,
                     "[ArZoom] Shortcut observer unavailable (Win32 %lu); "
                     "other presentation effects remain available",
                     GetLastError());
            else
                blog(LOG_INFO,
                     "[ArZoom] Optional keyboard shortcut observer active");
        }
    };
    sync_hook();
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        sync_hook();
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (hook) UnhookWindowsHookEx(hook);
    keys_down.fill(false);
    right_alt_down = false;
    hook_thread_id.store(0, std::memory_order_release);
}

void wake_hook() noexcept
{
    const DWORD id = hook_thread_id.load(std::memory_order_acquire);
    if (id) PostThreadMessageW(id, kObserverWake, 0, 0);
}

} // namespace

bool subscribe()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (user_count != 0) {
        ++user_count;
        return true;
    }
    opt_in_active.store(true, std::memory_order_release);
    published.store(0, std::memory_order_release);
    if (!worker.joinable()) {
        /* Spawn without waiting for the hook thread's message queue:
         * an OBS video_tick must NEVER block on observer startup. The worker
         * primes its own queue and checks opt_in_active on entry. */
        try {
            worker = std::thread(observer_thread);
        } catch (const std::system_error &) {
            opt_in_active.store(false, std::memory_order_release);
            blog(LOG_WARNING,
                 "[ArZoom] Could not start shortcut observer worker");
            return false;
        }
    }
    user_count = 1;
    wake_hook();
    return true;
}

void unsubscribe()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (user_count == 0 || --user_count != 0) return;
    opt_in_active.store(false, std::memory_order_release);
    published.store(0, std::memory_order_release);
    wake_hook();
}

Snapshot snapshot() noexcept
{
    const auto value = published.load(std::memory_order_acquire);
    if (!value || !opt_in_active.load(std::memory_order_acquire))
        return {};
    Snapshot result;
    result.generation = static_cast<std::uint32_t>(value >> 32);
    result.chord.modifiers =
        static_cast<std::uint8_t>((value >> 16) & 0xff);
    result.chord.key =
        static_cast<presentation::ShortcutKey>(value & 0xffff);
    return result;
}

void shutdown()
{
    std::thread old;
    {
        /* A worker may still be booting because subscribe is nonblocking.
         * Waiting is allowed only here, never from video_tick/render. */
        std::unique_lock<std::mutex> lock(lifecycle_mutex);
        user_count = 0;
        opt_in_active.store(false, std::memory_order_release);
        published.store(0, std::memory_order_release);
        if (worker.joinable()) {
            ready_cv.wait(lock, [] { return message_queue_ready; });
            const DWORD id = hook_thread_id.load(std::memory_order_acquire);
            if (id) PostThreadMessageW(id, WM_QUIT, 0, 0);
            old = std::move(worker);
        }
    }
    if (old.joinable()) old.join();
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    message_queue_ready = false;
}

} // namespace arzoom::shortcut

#else
namespace arzoom::shortcut {
bool subscribe() { return false; }
void unsubscribe() {}
Snapshot snapshot() noexcept { return {}; }
void shutdown() {}
} // namespace arzoom::shortcut
#endif
