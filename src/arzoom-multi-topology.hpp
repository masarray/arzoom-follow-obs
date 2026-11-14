#pragma once

#include "arzoom-multi-coordinate.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>
#include <type_traits>

namespace arzoom {

inline constexpr std::size_t kMultiSourceUuidCapacity = 64;

struct MultiSourceUuid {
    std::array<char, kMultiSourceUuidCapacity> bytes{};

    constexpr bool valid() const { return bytes[0] != '\0'; }
};

inline MultiSourceUuid multi_source_uuid_from_string(std::string_view value)
{
    MultiSourceUuid result;
    if (value.empty() || value.size() >= result.bytes.size())
        return result;

    for (std::size_t i = 0; i < value.size(); ++i)
        result.bytes[i] = value[i];
    result.bytes[value.size()] = '\0';
    return result;
}

inline bool multi_source_uuid_equal(const MultiSourceUuid &a,
                                    const MultiSourceUuid &b)
{
    return a.bytes == b.bytes;
}

struct MultiRawPresentationScreen {
    MultiSourceUuid uuid{};
    bool eligible = false;
    bool visible = false;
    MultiPhysicalRect physical{};
    double source_width = 0.0;
    double source_height = 0.0;
    MultiCrop crop{};
    MultiSceneRect scene_rect{};
};

struct MultiRawTopologySnapshot {
    std::uint64_t generation = 0;
    std::uint8_t count = 0;
    std::array<MultiRawPresentationScreen, kMultiMaxPresentationScreens>
        screens{};
};

enum class MultiTopologyStatus : std::uint8_t {
    Ready,
    Empty,
    TooManyScreens,
    InvalidGeneration,
};

enum class MultiCanonicalScreenStatus : std::uint8_t {
    Ready,
    Ineligible,
    Hidden,
    InvalidIdentity,
    InvalidGeometry,
};

struct MultiCanonicalScreen {
    MultiSourceUuid uuid{};
    MultiCanonicalScreenStatus status =
        MultiCanonicalScreenStatus::Ineligible;
    MultiGeometryStatus geometry_status = MultiGeometryStatus::Ready;
    MultiPresentationScreen mapping{};
};

struct MultiCanonicalTopology {
    std::uint64_t generation = 0;
    std::uint8_t count = 0;
    MultiTopologyStatus status = MultiTopologyStatus::Empty;
    std::array<MultiCanonicalScreen, kMultiMaxPresentationScreens> screens{};
};

inline MultiCanonicalTopology multi_prepare_topology(
    const MultiRawTopologySnapshot &raw)
{
    MultiCanonicalTopology result;
    result.generation = raw.generation;

    if (raw.generation == 0) {
        result.status = MultiTopologyStatus::InvalidGeneration;
        return result;
    }
    if (raw.count > kMultiMaxPresentationScreens) {
        result.status = MultiTopologyStatus::TooManyScreens;
        return result;
    }
    if (raw.count == 0) {
        result.status = MultiTopologyStatus::Empty;
        return result;
    }

    result.count = raw.count;
    result.status = MultiTopologyStatus::Ready;

    for (std::size_t i = 0; i < raw.count; ++i) {
        const MultiRawPresentationScreen &input = raw.screens[i];
        MultiCanonicalScreen &output = result.screens[i];

        output.uuid = input.uuid;
        output.mapping.physical = input.physical;
        output.mapping.source_width = input.source_width;
        output.mapping.source_height = input.source_height;
        output.mapping.crop = input.crop;

        if (!input.eligible) {
            output.status = MultiCanonicalScreenStatus::Ineligible;
            continue;
        }
        if (!input.visible) {
            output.status = MultiCanonicalScreenStatus::Hidden;
            continue;
        }
        if (!input.uuid.valid()) {
            output.status = MultiCanonicalScreenStatus::InvalidIdentity;
            continue;
        }

        const MultiTransformBuildResult built =
            multi_build_axis_aligned_transform(input.source_width,
                                               input.source_height,
                                               input.crop,
                                               input.scene_rect);
        if (!built.ready()) {
            output.status = MultiCanonicalScreenStatus::InvalidGeometry;
            output.geometry_status = built.status;
            continue;
        }

        output.mapping.source_to_scene = built.transform;
        output.geometry_status =
            multi_screen_geometry_status(output.mapping);
        if (output.geometry_status != MultiGeometryStatus::Ready) {
            output.status = MultiCanonicalScreenStatus::InvalidGeometry;
            continue;
        }

        output.mapping.eligible = true;
        output.status = MultiCanonicalScreenStatus::Ready;
    }

    return result;
}

/* Data-race-free coherent publication without a mutex in the hot reader.
 *
 * A reader pins the currently published slot with a tiny atomic reader count.
 * The single writer only rewrites the inactive slot after its old readers have
 * drained, then publishes that complete slot with one release store.
 */
class MultiTopologyPublisher {
public:
    MultiTopologyPublisher()
    {
        reader_counts_[0].store(0, std::memory_order_relaxed);
        reader_counts_[1].store(0, std::memory_order_relaxed);
    }

    MultiTopologyPublisher(const MultiTopologyPublisher &) = delete;
    MultiTopologyPublisher &operator=(const MultiTopologyPublisher &) = delete;

    MultiCanonicalTopology read() const
    {
        for (;;) {
            const unsigned index =
                published_index_.load(std::memory_order_acquire);

            reader_counts_[index].fetch_add(1, std::memory_order_acq_rel);

            if (index !=
                published_index_.load(std::memory_order_acquire)) {
                reader_counts_[index].fetch_sub(
                    1, std::memory_order_release);
                continue;
            }

            const MultiCanonicalTopology snapshot = slots_[index];

            reader_counts_[index].fetch_sub(
                1, std::memory_order_release);
            return snapshot;
        }
    }

    void publish(const MultiCanonicalTopology &snapshot)
    {
        const unsigned current =
            published_index_.load(std::memory_order_acquire);
        const unsigned next = current ^ 1U;

        while (reader_counts_[next].load(std::memory_order_acquire) != 0)
            std::this_thread::yield();

        slots_[next] = snapshot;
        published_index_.store(next, std::memory_order_release);
    }

private:
    std::array<MultiCanonicalTopology, 2> slots_{};
    std::atomic<unsigned> published_index_{0};
    mutable std::array<std::atomic<std::uint32_t>, 2> reader_counts_;
};

struct MultiTopologyWorkerStats {
    std::uint64_t requests_received = 0;
    std::uint64_t requests_ignored = 0;
    std::uint64_t requests_coalesced = 0;
    std::uint64_t builds_started = 0;
    std::uint64_t stale_builds_discarded = 0;
    std::uint64_t shutdown_builds_discarded = 0;
    std::uint64_t builds_published = 0;
    std::uint64_t last_published_generation = 0;
};

using MultiTopologyPrepareHook =
    void (*)(const MultiRawTopologySnapshot &, void *);

class MultiTopologyWorker {
public:
    explicit MultiTopologyWorker(MultiTopologyPrepareHook hook = nullptr,
                                 void *hook_context = nullptr)
        : hook_(hook), hook_context_(hook_context)
    {
    }

    MultiTopologyWorker(const MultiTopologyWorker &) = delete;
    MultiTopologyWorker &operator=(const MultiTopologyWorker &) = delete;

    ~MultiTopologyWorker() { stop(); }

    bool start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_)
            return false;

        stopping_ = false;
        active_ = false;
        has_pending_ = false;
        highest_requested_generation_ = 0;
        stats_ = {};
        running_ = true;

        thread_ = std::thread([this]() { run(); });
        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_)
                return;

            stopping_ = true;
            has_pending_ = false;
        }

        work_cv_.notify_all();
        state_cv_.notify_all();

        if (thread_.joinable())
            thread_.join();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            active_ = false;
            has_pending_ = false;
        }

        state_cv_.notify_all();
    }

    bool submit(const MultiRawTopologySnapshot &snapshot)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_)
            return false;

        ++stats_.requests_received;

        if (snapshot.generation == 0 ||
            snapshot.generation <= highest_requested_generation_) {
            ++stats_.requests_ignored;
            return false;
        }

        if (has_pending_)
            ++stats_.requests_coalesced;

        pending_ = snapshot;
        has_pending_ = true;
        highest_requested_generation_ = snapshot.generation;

        work_cv_.notify_one();
        return true;
    }

    MultiCanonicalTopology snapshot() const
    {
        return publisher_.read();
    }

    MultiTopologyWorkerStats stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    bool wait_until_published(std::uint64_t generation,
                              std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        const bool reached =
            state_cv_.wait_for(lock, timeout, [this, generation]() {
                return stats_.last_published_generation >= generation ||
                       (!running_ && !active_ && !has_pending_);
            });

        return reached &&
               stats_.last_published_generation >= generation;
    }

    bool wait_until_idle(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return state_cv_.wait_for(lock, timeout, [this]() {
            return !active_ && !has_pending_;
        });
    }

private:
    void run()
    {
        for (;;) {
            MultiRawTopologySnapshot raw;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                work_cv_.wait(lock, [this]() {
                    return stopping_ || has_pending_;
                });

                if (stopping_)
                    break;

                raw = pending_;
                has_pending_ = false;
                active_ = true;
                ++stats_.builds_started;
            }

            if (hook_)
                hook_(raw, hook_context_);

            const MultiCanonicalTopology candidate =
                multi_prepare_topology(raw);

            {
                std::unique_lock<std::mutex> lock(mutex_);
                active_ = false;

                if (stopping_) {
                    ++stats_.shutdown_builds_discarded;
                    state_cv_.notify_all();
                    break;
                }

                if (raw.generation !=
                    highest_requested_generation_) {
                    ++stats_.stale_builds_discarded;
                    state_cv_.notify_all();
                    continue;
                }

                /* This bounded publication stays under the submit mutex so a
                 * newer request cannot race between the latest-generation
                 * check and publication. Hot readers never take this mutex. */
                publisher_.publish(candidate);

                ++stats_.builds_published;
                stats_.last_published_generation =
                    candidate.generation;
                state_cv_.notify_all();
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        state_cv_.notify_all();
    }

    mutable std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable state_cv_;
    std::thread thread_;

    bool running_ = false;
    bool stopping_ = false;
    bool active_ = false;
    bool has_pending_ = false;

    std::uint64_t highest_requested_generation_ = 0;
    MultiRawTopologySnapshot pending_{};
    MultiTopologyWorkerStats stats_{};
    MultiTopologyPublisher publisher_{};

    MultiTopologyPrepareHook hook_ = nullptr;
    void *hook_context_ = nullptr;
};

static_assert(std::is_trivially_copyable<MultiSourceUuid>::value,
              "Multi UUID must remain fixed-size value state");
static_assert(
    std::is_trivially_copyable<MultiRawTopologySnapshot>::value,
    "Raw topology must remain value-only/trivially copyable");
static_assert(
    std::is_trivially_copyable<MultiCanonicalTopology>::value,
    "Canonical topology must remain value-only/trivially copyable");

} // namespace arzoom
