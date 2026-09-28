#include "../src/arzoom-multi-topology.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

arzoom::MultiRawTopologySnapshot make_raw(std::uint64_t generation)
{
    arzoom::MultiRawTopologySnapshot raw;
    raw.generation = generation;
    raw.count = 1;

    auto &screen = raw.screens[0];
    screen.uuid =
        arzoom::multi_source_uuid_from_string("test-display-uuid-a");
    screen.eligible = true;
    screen.visible = true;
    screen.physical = {0, 0, 1920, 1080};
    screen.source_width = 1920.0;
    screen.source_height = 1080.0;
    screen.crop = {};
    screen.scene_rect = {100.0, 50.0, 1060.0, 590.0};
    return raw;
}

struct PrepareGate {
    std::mutex mutex;
    std::condition_variable entered_cv;
    std::condition_variable release_cv;
    bool entered = false;
    bool release = false;
};

void blocking_generation_one_hook(
    const arzoom::MultiRawTopologySnapshot &raw, void *context)
{
    if (raw.generation != 1)
        return;

    auto *gate = static_cast<PrepareGate *>(context);
    std::unique_lock<std::mutex> lock(gate->mutex);
    gate->entered = true;
    gate->entered_cv.notify_all();
    gate->release_cv.wait(lock, [gate]() { return gate->release; });
}

void raw_preparation_is_bounded_and_value_only()
{
    const auto raw = make_raw(7);
    const auto canonical = arzoom::multi_prepare_topology(raw);

    require(canonical.generation == 7,
            "prepared topology lost generation");
    require(canonical.status ==
                arzoom::MultiTopologyStatus::Ready,
            "valid raw topology did not prepare");
    require(canonical.count == 1,
            "prepared topology count changed");
    require(canonical.screens[0].status ==
                arzoom::MultiCanonicalScreenStatus::Ready,
            "valid screen did not become canonical Ready");
    require(canonical.screens[0].mapping.eligible,
            "valid selected screen lost eligibility");
    require(arzoom::multi_source_uuid_equal(
                raw.screens[0].uuid,
                canonical.screens[0].uuid),
            "durable UUID value was not carried into canonical state");

    auto hidden = raw;
    hidden.generation = 8;
    hidden.screens[0].visible = false;
    const auto hidden_canonical =
        arzoom::multi_prepare_topology(hidden);
    require(hidden_canonical.screens[0].status ==
                arzoom::MultiCanonicalScreenStatus::Hidden,
            "hidden screen did not fail safe");
    require(!hidden_canonical.screens[0].mapping.eligible,
            "hidden screen remained runtime-eligible");

    auto no_identity = raw;
    no_identity.generation = 9;
    no_identity.screens[0].uuid = {};
    const auto no_identity_canonical =
        arzoom::multi_prepare_topology(no_identity);
    require(no_identity_canonical.screens[0].status ==
                arzoom::MultiCanonicalScreenStatus::InvalidIdentity,
            "missing durable UUID was accepted");
}

void burst_coalesces_to_active_plus_latest_only()
{
    PrepareGate gate;
    arzoom::MultiTopologyWorker worker(
        blocking_generation_one_hook, &gate);

    require(worker.start(), "worker did not start");
    require(worker.submit(make_raw(1)),
            "generation 1 was not accepted");

    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        gate.entered_cv.wait(
            lock, [&gate]() { return gate.entered; });
    }

    for (std::uint64_t generation = 2;
         generation <= 1001; ++generation) {
        require(worker.submit(make_raw(generation)),
                "burst generation was rejected");
    }

    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.release = true;
    }
    gate.release_cv.notify_all();

    require(worker.wait_until_published(1001, 5s),
            "latest burst generation was not published");

    const auto stats = worker.stats();
    require(stats.requests_received == 1001,
            "request accounting changed");
    require(stats.builds_started == 2,
            "worker rebuilt obsolete intermediate generations");
    require(stats.stale_builds_discarded == 1,
            "active stale generation was not discarded");
    require(stats.builds_published == 1,
            "stale generation reached publication");
    require(stats.requests_coalesced == 999,
            "single pending slot did not coalesce the burst");
    require(worker.snapshot().generation == 1001,
            "published snapshot is not the latest generation");

    worker.stop();
}

void stale_or_older_generation_never_overwrites_latest()
{
    arzoom::MultiTopologyWorker worker;
    require(worker.start(), "monotonic worker did not start");

    require(worker.submit(make_raw(5)),
            "generation 5 was not accepted");
    require(!worker.submit(make_raw(4)),
            "older generation was accepted after newer request");

    require(worker.wait_until_published(5, 5s),
            "generation 5 did not publish");

    const auto stats = worker.stats();
    require(stats.requests_ignored == 1,
            "older generation was not counted as ignored");
    require(worker.snapshot().generation == 5,
            "older generation overwrote canonical state");

    worker.stop();
}

void shutdown_during_prepare_joins_and_does_not_publish()
{
    PrepareGate gate;
    arzoom::MultiTopologyWorker worker(
        blocking_generation_one_hook, &gate);

    require(worker.start(), "shutdown worker did not start");
    require(worker.submit(make_raw(1)),
            "shutdown generation was not accepted");

    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        gate.entered_cv.wait(
            lock, [&gate]() { return gate.entered; });
    }

    std::atomic<bool> stop_started{false};
    std::atomic<bool> stop_returned{false};

    std::thread stopper([&worker, &stop_started, &stop_returned]() {
        stop_started.store(true, std::memory_order_release);
        worker.stop();
        stop_returned.store(true, std::memory_order_release);
    });

    while (!stop_started.load(std::memory_order_acquire))
        std::this_thread::yield();

    require(!stop_returned.load(std::memory_order_acquire),
            "stop returned while preparation hook was still blocked");

    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.release = true;
    }
    gate.release_cv.notify_all();
    stopper.join();

    require(stop_returned.load(std::memory_order_acquire),
            "stop did not deterministically join the worker");

    const auto stats = worker.stats();
    require(stats.builds_published == 0,
            "shutdown generation was published");
    require(stats.shutdown_builds_discarded == 1,
            "shutdown discard was not recorded");
}

void invalid_latest_topology_publishes_fail_safe_status()
{
    auto over_cap = make_raw(9);
    over_cap.count =
        static_cast<std::uint8_t>(
            arzoom::kMultiMaxPresentationScreens + 1);

    const auto pure = arzoom::multi_prepare_topology(over_cap);
    require(pure.status ==
                arzoom::MultiTopologyStatus::TooManyScreens,
            "screen-cap overflow did not fail safe");

    arzoom::MultiTopologyWorker worker;
    require(worker.start(), "invalid-topology worker did not start");
    require(worker.submit(over_cap),
            "latest invalid generation was rejected");

    require(worker.wait_until_published(9, 5s),
            "latest invalid generation did not publish fail-safe state");

    const auto published = worker.snapshot();
    require(published.generation == 9,
            "fail-safe publication lost latest generation");
    require(published.status ==
                arzoom::MultiTopologyStatus::TooManyScreens,
            "last-known topology was silently retained instead of "
            "publishing current unavailable state");

    worker.stop();
}

void coherent_publication_never_exposes_torn_topology()
{
    arzoom::MultiTopologyPublisher publisher;
    std::atomic<bool> done{false};
    std::atomic<bool> coherent{true};

    std::thread reader([&publisher, &done, &coherent]() {
        do {
            const auto snapshot = publisher.read();
            if (snapshot.generation == 0)
                continue;

            if (snapshot.status !=
                    arzoom::MultiTopologyStatus::Ready ||
                snapshot.count != arzoom::kMultiMaxPresentationScreens) {
                coherent.store(false, std::memory_order_release);
                break;
            }

            for (std::size_t i = 0;
                 i < arzoom::kMultiMaxPresentationScreens; ++i) {
                const double expected =
                    static_cast<double>(
                        snapshot.generation * 100 + i);

                if (snapshot.screens[i].mapping.source_width !=
                        expected ||
                    snapshot.screens[i].mapping.source_height !=
                        expected + 0.5) {
                    coherent.store(false, std::memory_order_release);
                    break;
                }
            }
        } while (!done.load(std::memory_order_acquire));
    });

    for (std::uint64_t generation = 1;
         generation <= 5000; ++generation) {
        arzoom::MultiCanonicalTopology snapshot;
        snapshot.generation = generation;
        snapshot.count =
            static_cast<std::uint8_t>(
                arzoom::kMultiMaxPresentationScreens);
        snapshot.status = arzoom::MultiTopologyStatus::Ready;

        for (std::size_t i = 0;
             i < arzoom::kMultiMaxPresentationScreens; ++i) {
            snapshot.screens[i].status =
                arzoom::MultiCanonicalScreenStatus::Ready;
            snapshot.screens[i].mapping.source_width =
                static_cast<double>(generation * 100 + i);
            snapshot.screens[i].mapping.source_height =
                static_cast<double>(generation * 100 + i) + 0.5;
        }

        publisher.publish(snapshot);
    }

    done.store(true, std::memory_order_release);
    reader.join();

    require(coherent.load(std::memory_order_acquire),
            "reader observed a torn old/new topology");

    const auto final_snapshot = publisher.read();
    require(final_snapshot.generation == 5000,
            "final coherent generation was not published");
}

void worker_waits_instead_of_busy_spinning_when_idle()
{
    arzoom::MultiTopologyWorker worker;
    require(worker.start(), "idle worker did not start");

    require(worker.wait_until_idle(1s),
            "fresh worker did not reach idle state");

    const auto before = worker.stats();
    std::this_thread::yield();
    std::this_thread::yield();
    std::this_thread::yield();
    const auto after = worker.stats();

    require(before.builds_started == after.builds_started,
            "idle worker performed phantom builds");
    require(before.builds_published == after.builds_published,
            "idle worker performed phantom publications");

    worker.stop();
}

} // namespace

int main()
{
    raw_preparation_is_bounded_and_value_only();
    burst_coalesces_to_active_plus_latest_only();
    stale_or_older_generation_never_overwrites_latest();
    shutdown_during_prepare_joins_and_does_not_publish();
    invalid_latest_topology_publishes_fail_safe_status();
    coherent_publication_never_exposes_torn_topology();
    worker_waits_instead_of_busy_spinning_when_idle();

    std::cout << "ArZoom M2A topology worker core gates: PASS\n";
    return 0;
}
