// Tests of the console commands that show and hide the views for debugging: "fps" and "hitboxes".

#include "Commands/Commands.hpp"
#include "Commands/FpsCommand.hpp"
#include "Commands/HitboxesCommand.hpp"
#include "Console/Console.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::client::Console;

TEST_CASE("The fps command switches the counter, or sets it with on and off", "[console]")
{
    Console console;
    bool shown = false;
    rtype::client::registerFpsCommand(console, shown);

    console.execute("fps");
    CHECK(shown);
    console.execute("fps");
    CHECK_FALSE(shown);
    console.execute("fps on");
    console.execute("fps on");
    CHECK(shown);
    console.execute("fps off");
    CHECK_FALSE(shown);
}

TEST_CASE("Wrong arguments change nothing and print the usage", "[console]")
{
    Console console;
    bool fps = true;
    bool hitboxes = false;
    rtype::client::registerFpsCommand(console, fps);
    rtype::client::registerHitboxesCommand(console, hitboxes);

    console.execute("fps maybe");
    console.execute("hitboxes on off");

    CHECK(fps);
    CHECK_FALSE(hitboxes);
    CHECK(console.getLines().back() == "Usage: hitboxes [on|off]");
}
