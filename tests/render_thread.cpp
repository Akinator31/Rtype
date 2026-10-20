#include "ThreadedInput.hpp"
#include "rtype/render/RenderCommands.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <thread>

TEST_CASE("Render queries execute on the thread that drains them", "[render-thread]")
{
    rtype::render::RenderQueries queries;
    const std::thread::id owner = std::this_thread::get_id();
    std::atomic<bool> submitted = false;
    std::thread::id executed;
    std::atomic<int> result = 0;
    std::thread worker([&] {
        submitted = true;
        result = queries.run([&] {
            executed = std::this_thread::get_id();
            return 42;
        });
    });
    while (!submitted.load()) {
        std::this_thread::yield();
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && result.load() == 0) {
        queries.drain();
        std::this_thread::yield();
    }
    worker.join();
    CHECK(result.load() == 42);
    CHECK(executed == owner);
}

TEST_CASE("A pending render frame is replaced by the newest complete frame", "[render-thread]")
{
    rtype::render::RenderFrames frames;
    frames.publish({[](rtype::render::Renderer &) {}});
    frames.publish({[](rtype::render::Renderer &) {}, [](rtype::render::Renderer &) {}});
    const auto latest = frames.take();
    REQUIRE(latest.has_value());
    CHECK(latest->size() == 2);
    CHECK_FALSE(frames.take().has_value());
}

TEST_CASE("Input mailbox merges events when logic misses a render frame", "[render-thread]")
{
    rtype::client::InputMailbox mailbox;
    rtype::client::InputFrame pressed;
    pressed.keyboard[static_cast<std::size_t>(rtype::input::Key::Space)] = true;
    pressed.typed = U"a";
    pressed.wheel = 1.0f;
    pressed.elapsed = 0.01f;
    mailbox.publish(std::move(pressed));

    rtype::client::InputFrame released;
    released.typed = U"b";
    released.wheel = 2.0f;
    released.elapsed = 0.02f;
    mailbox.publish(std::move(released));

    const auto merged = mailbox.take();
    REQUIRE(merged.has_value());
    CHECK(merged->keyboard[static_cast<std::size_t>(rtype::input::Key::Space)]);
    CHECK(merged->typed == U"ab");
    CHECK(merged->wheel == 3.0f);
    CHECK(std::abs(merged->elapsed - 0.03f) < 0.00001f);
    mailbox.publish(rtype::client::InputFrame{});
    const auto next = mailbox.take();
    REQUIRE(next.has_value());
    CHECK_FALSE(next->keyboard[static_cast<std::size_t>(rtype::input::Key::Space)]);
    mailbox.stop();
    CHECK_FALSE(mailbox.take().has_value());
}
