// Tests of Orbit Breaker, the brick breaker made with the engine alone: its rules, then whole games played by
// its systems on a fake keyboard, with the audio disabled and no window.

#include "Arena.hpp"
#include "BallSystem.hpp"
#include "Bounce.hpp"
#include "Effects.hpp"
#include "FakeInput.hpp"
#include "Levels.hpp"
#include "Looks.hpp"
#include "Systems.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/ThreadManager.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <set>
#include <stdexcept>

using namespace orbit;
using Catch::Approx;
using rtype::input::Key;

TEST_CASE("The ball is mirrored on what it goes into, and keeps going away from the rest", "[orbit]")
{
    CHECK(reflect({300.0f, -400.0f}, {0.0f, -1.0f}) == Vector{300.0f, 400.0f});
    CHECK(reflect({300.0f, -400.0f}, {-1.0f, 0.0f}) == Vector{300.0f, -400.0f});
    CHECK(reflect({300.0f, 400.0f}, {1.0f, 0.0f}) == Vector{-300.0f, 400.0f});
}

TEST_CASE("The paddle sends the ball up, slanted towards its edges", "[orbit]")
{
    const Vector middle = leavePaddle(0.0f, 600.0f);
    CHECK(middle.x == Approx(0.0f).margin(0.001f));
    CHECK(middle.y == Approx(-600.0f));
    const Vector edge = leavePaddle(1.0f, 600.0f);
    CHECK(edge.x > 0.0f);
    CHECK(edge.y < 0.0f);
    CHECK(std::hypot(edge.x, edge.y) == Approx(600.0f));
    CHECK(leavePaddle(5.0f, 600.0f) == edge); // past the edge
    CHECK(leavePaddle(-1.0f, 600.0f).x == Approx(-edge.x));
}

TEST_CASE("Each brick in a row since the paddle is worth more, up to a limit", "[orbit]")
{
    CHECK(hitPoints(1) == HitScore);
    CHECK(hitPoints(3) == 3 * HitScore);
    CHECK(hitPoints(50) == MaxCombo * HitScore);
}

TEST_CASE("The ball speeds up after each brick, up to a limit", "[orbit]")
{
    CHECK(speedUp(BallStartSpeed) > BallStartSpeed);
    CHECK(speedUp(BallMaxSpeed) == BallMaxSpeed);
}

TEST_CASE("The levels grow, get stronger, and stay in the arena", "[orbit]")
{
    const auto strongest = [](const std::vector<BrickSpec> &bricks) {
        int strength = 0;
        for (const BrickSpec &brick : bricks) {
            strength = std::max(strength, brick.strength);
            CHECK(brick.x - (BrickWidth / 2.0f) > WallThickness);
            CHECK(brick.x + (BrickWidth / 2.0f) < ArenaWidth - WallThickness);
            CHECK(brick.y > FieldTop + WallThickness);
            CHECK(brick.y < PaddleY - 300.0f);
        }
        return strength;
    };
    CHECK(levelLayout(1).size() == 4 * BrickColumns);
    CHECK(levelLayout(2).size() < 5 * BrickColumns); // holes on even levels
    CHECK(levelLayout(9).size() == 8 * BrickColumns);
    CHECK(strongest(levelLayout(1)) < strongest(levelLayout(7)));
    CHECK(strongest(levelLayout(7)) == MaxBrickStrength);
}

namespace
{

/// A game in an open scene, run by the systems of the game on a fake keyboard, 60 frames per second.
struct Game {
    std::filesystem::path sounds = std::filesystem::temp_directory_path() / "rtype_tests_orbit_breaker";
    FakeInput input;
    rtype::engine::Engine engine;
    rtype::audio::AudioDevice device{{.enabled = false}};
    rtype::engine::ThreadManager threads;
    rtype::audio::AudioPlayer audio{device, sounds, threads.getAssetsWorker()};
    Match match;
    Looks looks;
    int gameOvers = 0;
    rtype::ecs::entity_t paddle = 0;

    Game()
    {
        // Audio is disabled: the sounds only have to exist
        std::filesystem::create_directories(sounds);
        std::ofstream(sounds / "sound.wav") << "fake";
        const rtype::audio::SoundId sound = audio.loadSound("sound.wav");
        engine.setInputSource(input);
        registerGameSystems(engine, audio, match, {sound, sound, sound}, looks, [this] { ++gameOvers; });
        rtype::ecs::Ecs &ecs = engine.getEcs();
        const rtype::engine::SceneId scene = engine.getScenes().open();
        spawnWalls(ecs, scene);
        paddle = spawnPaddle(ecs, scene, looks);
        spawnBall(ecs, scene, looks);
        spawnBricks(ecs, scene, match.level);
    }
    ~Game()
    {
        threads.stopAll();
        std::filesystem::remove_all(sounds);
    }

    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    void frames(int count, std::set<Key> held = {})
    {
        input.keys = std::move(held);
        for (int frame = 0; frame < count; ++frame) {
            engine.update(1.0f / 60.0f);
        }
    }

    /// The first entity with this component: the game has one paddle and one ball.
    template <typename Component>
    [[nodiscard]] rtype::ecs::entity_t entityOf()
    {
        auto &cache = engine.getEcs().cache<Component>();
        for (rtype::ecs::entity_t entity = 0; entity < cache.size(); ++entity) {
            if (cache.has(entity)) {
                return entity;
            }
        }
        throw std::logic_error("No such component");
    }
    template <typename Component>
    [[nodiscard]] Component &the()
    {
        return engine.getEcs().getComponent<Component>(entityOf<Component>());
    }
    [[nodiscard]] rtype::ecs::Position &position(rtype::ecs::entity_t entity)
    {
        return engine.getEcs().getComponent<rtype::ecs::Position>(entity);
    }
    [[nodiscard]] rtype::ecs::Position &ball() { return position(entityOf<Ball>()); }
    template <typename Component>
    [[nodiscard]] std::size_t count()
    {
        std::size_t count = 0;
        auto &cache = engine.getEcs().cache<Component>();
        for (rtype::ecs::entity_t entity = 0; entity < cache.size(); ++entity) {
            count += cache.has(entity) ? 1 : 0;
        }
        return count;
    }
    [[nodiscard]] std::size_t bricks() { return count<Brick>(); }

    /// Where the paddle must be, under the ball, to send it straight to a brick left: leavePaddle() turned
    /// around.
    [[nodiscard]] float aimAtBrick()
    {
        const rtype::ecs::Position &target = position(entityOf<Brick>());
        const float angle = std::atan2(target.x - ball().x, ball().y - target.y);
        const float offset = std::clamp(angle / (std::numbers::pi_v<float> / 3.0f), -1.0f, 1.0f);
        return ball().x - (offset * PaddleWidth / 2.0f);
    }

    /// Plays with a perfect paddle, always under the ball and aiming at a brick, until the condition or the
    /// time limit.
    template <typename Until>
    void playWell(Until until, int maxFrames)
    {
        frames(1, {Key::Space});
        for (int frame = 0; frame < maxFrames && !until(); ++frame) {
            position(paddle).x = aimAtBrick();
            frames(1);
        }
    }

    /// Launches the ball, then holds the paddle against the left wall until the ball is lost, or the time
    /// limit.
    void missBall(int maxFrames)
    {
        const int lives = match.lives;
        frames(1, {Key::Space});
        for (int frame = 0; frame < maxFrames && match.lives == lives; ++frame) {
            frames(1, {Key::Left});
        }
    }
};

} // namespace

TEST_CASE("The ball waits on the paddle, follows it, and Fire launches it upwards", "[orbit]")
{
    Game game;
    game.frames(30, {Key::Right});
    CHECK(game.position(game.paddle).x > ArenaWidth / 2.0f);
    CHECK(game.ball().x == Approx(game.position(game.paddle).x));
    CHECK_FALSE(game.the<Ball>().launched);

    game.frames(1, {Key::Space});
    game.frames(5);

    CHECK(game.the<Ball>().launched);
    CHECK_FALSE(game.match.ballWaiting);
    CHECK(game.ball().y < PaddleY - BallSize);
}

TEST_CASE("The walls stop the paddle", "[orbit]")
{
    Game game;
    game.frames(180, {Key::Left});

    CHECK(game.position(game.paddle).x - (PaddleWidth / 2.0f) >= WallThickness - 1.0f);
}

TEST_CASE("A ball kept in play breaks bricks, scores, and goes faster", "[orbit]")
{
    Game game;
    const std::size_t bricks = game.bricks();

    game.playWell([&game] { return game.match.score >= 10 * HitScore; }, 60 * 60);

    CHECK(game.match.score >= 10 * HitScore);
    CHECK(game.bricks() < bricks);
    CHECK(game.the<Ball>().speed > BallStartSpeed);
    CHECK(game.match.lives == StartingLives);
    // The trail, the sparks and the points fade and go: they do not pile up
    CHECK(game.count<Fade>() > 0);
    CHECK(game.count<Fade>() < 100);
}

TEST_CASE("A ball missed costs a life, and the last one ends the game once", "[orbit]")
{
    Game game;
    for (int life = 0; life < StartingLives; ++life) {
        game.missBall(60 * 60);
    }
    game.frames(5);

    CHECK(game.match.lives == 0);
    CHECK(game.gameOvers == 1);
}

TEST_CASE("Breaking every brick starts the next level with the ball back on the paddle", "[orbit]")
{
    Game game;
    game.frames(1, {Key::Space});
    rtype::ecs::Ecs &ecs = game.engine.getEcs();
    auto &bricks = ecs.cache<Brick>();
    for (rtype::ecs::entity_t brick = 0; brick < bricks.size(); ++brick) {
        if (bricks.has(brick)) {
            ecs.destroyEntity(brick);
        }
    }

    game.frames(1);

    CHECK(game.match.level == 2);
    CHECK(game.bricks() == levelLayout(2).size());
    CHECK_FALSE(game.the<Ball>().launched);
}

TEST_CASE("A good player clears the first level by playing it", "[orbit]")
{
    Game game;

    game.playWell([&game] { return game.match.level == 2; }, 10 * 60 * 60);

    CHECK(game.match.level == 2);
    CHECK(game.match.lives == StartingLives);
    CHECK(game.bricks() == levelLayout(2).size());
}
