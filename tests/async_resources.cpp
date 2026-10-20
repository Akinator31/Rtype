#include "rtype/core/AsyncResourceRequests.hpp"
#include "rtype/core/Handle.hpp"
#include "rtype/core/ResourceCache.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

using Id = rtype::core::Handle<struct TestAsset>;

template <typename Future>
bool finishWithin(rtype::core::AsyncResourceRequests<Id> &requests, const Future &future)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready &&
           std::chrono::steady_clock::now() < deadline) {
        requests.pump();
        std::this_thread::yield();
    }
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

} // namespace

TEST_CASE("Assets prepare on a worker and finish on the requesting thread", "[assets][async]")
{
    rtype::core::AsyncResourceRequests<Id> requests;
    const std::thread::id owner = std::this_thread::get_id();
    std::thread::id worker;
    std::thread::id finisher;
    std::atomic<int> prepared = 0;
    std::atomic<int> finished = 0;

    const auto first = requests.request(
        "ship",
        [&] {
            worker = std::this_thread::get_id();
            ++prepared;
            return 42;
        },
        [&](int value) {
            finisher = std::this_thread::get_id();
            ++finished;
            return Id{static_cast<std::uint32_t>(value)};
        });
    const auto second = requests.request("ship", [] { return 99; }, [](int) { return Id{99}; });

    REQUIRE(finishWithin(requests, first));
    CHECK(first.get() == second.get());
    CHECK(first.get() == Id{42});
    CHECK(worker != owner);
    CHECK(finisher == owner);
    CHECK(prepared == 1);
    CHECK(finished == 1);
}

TEST_CASE("A failed asset request reports its error and can be retried", "[assets][async]")
{
    rtype::core::AsyncResourceRequests<Id> requests;
    const auto failed = requests.request(
        "missing", []() -> int { throw std::runtime_error("missing"); }, [](int) { return Id{0}; });
    REQUIRE(finishWithin(requests, failed));
    CHECK_THROWS_AS(failed.get(), std::runtime_error);

    const auto retry = requests.request(
        "missing", [] { return 7; }, [](int value) { return Id{static_cast<std::uint32_t>(value)}; });
    REQUIRE(finishWithin(requests, retry));
    CHECK(retry.get() == Id{7});
}

TEST_CASE("Concurrent cache requests for one asset share one stable handle", "[assets][async]")
{
    rtype::core::ResourceCache<std::string, Id> cache;
    std::atomic<int> loads = 0;
    std::vector<Id> ids(16);
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        threads.emplace_back([&, index] {
            ids[index] = cache.load("ship", [&] {
                ++loads;
                return std::string("ship");
            });
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(loads == 1);
    CHECK(cache.size() == 1);
    for (const Id id : ids) {
        CHECK(id == ids.front());
        CHECK(cache.get(id) == "ship");
    }
}
