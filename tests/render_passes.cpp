#include "rtype/render/PassOrder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using rtype::render::Pass;
using rtype::render::PassOrder;

TEST_CASE("A frame and an eye are drawn one after the other", "[render][passes]")
{
    PassOrder passes;
    REQUIRE(passes.current() == Pass::None);

    passes.beginFrame();
    REQUIRE(passes.current() == Pass::Frame);
    REQUIRE_NOTHROW(passes.requireFrame());
    passes.endFrame();

    passes.beginEye();
    REQUIRE(passes.current() == Pass::Eye);
    REQUIRE_NOTHROW(passes.requireEye());
    passes.endEye();
    REQUIRE(passes.current() == Pass::None);
}

TEST_CASE("A pass cannot start before the previous one ends", "[render][passes]")
{
    PassOrder passes;
    passes.beginFrame();
    REQUIRE_THROWS_AS(passes.beginFrame(), std::logic_error);
    REQUIRE_THROWS_AS(passes.beginEye(), std::logic_error);
    passes.endFrame();

    passes.beginEye();
    REQUIRE_THROWS_AS(passes.beginEye(), std::logic_error);
    REQUIRE_THROWS_AS(passes.beginFrame(), std::logic_error);
    // The refused starts changed nothing
    REQUIRE(passes.current() == Pass::Eye);
}

TEST_CASE("A pass is ended by its own end only", "[render][passes]")
{
    PassOrder passes;
    REQUIRE_THROWS_AS(passes.endFrame(), std::logic_error);
    REQUIRE_THROWS_AS(passes.endEye(), std::logic_error);

    passes.beginFrame();
    REQUIRE_THROWS_AS(passes.endEye(), std::logic_error);
    passes.endFrame();
    passes.beginEye();
    REQUIRE_THROWS_AS(passes.endFrame(), std::logic_error);
}

TEST_CASE("Each drawing needs the pass that draws it", "[render][passes]")
{
    PassOrder passes;
    REQUIRE_THROWS_AS(passes.requireFrame(), std::logic_error);
    REQUIRE_THROWS_AS(passes.requireEye(), std::logic_error);

    passes.beginFrame();
    REQUIRE_THROWS_AS(passes.requireEye(), std::logic_error);
    passes.endFrame();

    passes.beginEye();
    REQUIRE_THROWS_AS(passes.requireFrame(), std::logic_error);
}

TEST_CASE("A board is drawn inside an eye, ended before it", "[render][passes]")
{
    PassOrder passes;
    REQUIRE_THROWS_AS(passes.beginBoard(), std::logic_error);
    passes.beginFrame();
    REQUIRE_THROWS_AS(passes.beginBoard(), std::logic_error);
    passes.endFrame();

    passes.beginEye();
    passes.beginBoard();
    REQUIRE(passes.current() == Pass::Board);
    REQUIRE(passes.onBoard());
    REQUIRE_THROWS_AS(passes.beginBoard(), std::logic_error);
    REQUIRE_THROWS_AS(passes.beginEye(), std::logic_error);
    REQUIRE_THROWS_AS(passes.beginFrame(), std::logic_error);
    REQUIRE_THROWS_AS(passes.endEye(), std::logic_error);
    REQUIRE_THROWS_AS(passes.endFrame(), std::logic_error);

    passes.endBoard();
    REQUIRE(passes.current() == Pass::Eye);
    REQUIRE_FALSE(passes.onBoard());
    REQUIRE_THROWS_AS(passes.endBoard(), std::logic_error);
    passes.endEye();
}

TEST_CASE("A board draws what a frame draws, not what an eye does", "[render][passes]")
{
    PassOrder passes;
    passes.beginEye();
    passes.beginBoard();
    REQUIRE_NOTHROW(passes.requireFrame());
    REQUIRE_THROWS_AS(passes.requireEye(), std::logic_error);
    passes.endBoard();
    REQUIRE_NOTHROW(passes.requireEye());
    REQUIRE_THROWS_AS(passes.requireFrame(), std::logic_error);
}
