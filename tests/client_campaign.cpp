// Tests of the opening of the campaign in the client: its briefing, written letter by letter like a radio
// transmission by the TransmissionSystem.

#include "Campaign/Transmission.hpp"
#include "Campaign/TransmissionSystem.hpp"
#include "FakeInput.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Engine/Engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace rtype::client;
using rtype::ecs::entity_t;
using rtype::engine::SoundEffect;
using rtype::engine::Text;

namespace
{

/// A transmission of two lines, with a sound of its own for each kind of sound.
struct TestTransmission {
    rtype::engine::Engine engine;
    FakeInput input;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    rtype::engine::SceneId scene = engine.getScenes().open();
    entity_t transmission = 0;
    std::vector<entity_t> lines;
    int ended = 0;

    static constexpr TransmissionSounds Sounds{
        .statics = {1}, .beep = {2}, .connect = {3}, .type = {4}, .line = {5}, .end = {6}};

    explicit TestTransmission(const std::vector<std::string> &texts)
    {
        ecs.registerSystem<TransmissionSystem>(ecs, engine.getScenes(), engine.getControls(), input);
        Transmission state;
        state.signal = createText("SIGNAL", true);
        state.channel = createText("CHANNEL", true);
        state.over = createText("OVER", true);
        state.hint = createText("SKIP", true);
        state.skipHint = "SKIP";
        state.continueHint = "CONTINUE";
        state.sounds = Sounds;
        state.onEnd = [this] { ++ended; };
        for (const std::string &text : texts) {
            lines.push_back(createText("", false));
            state.lines.push_back({lines.back(), text, 10.0f});
        }
        transmission = ecs.createEntity();
        ecs.addComponent<rtype::engine::SceneMember>(transmission, scene);
        ecs.addComponent<Transmission>(transmission, std::move(state));
    }

    entity_t createText(const std::string &text, bool hidden)
    {
        const entity_t entity = ecs.createEntity();
        ecs.addComponent<rtype::engine::SceneMember>(entity, scene);
        ecs.addComponent<Text>(entity, text);
        ecs.addComponent<rtype::engine::Opacity>(entity);
        if (hidden) {
            ecs.addComponent<rtype::ecs::Hidden>(entity);
        }
        return entity;
    }

    [[nodiscard]] const Transmission &state() { return ecs.getComponent<Transmission>(transmission); }
    [[nodiscard]] bool hidden(entity_t entity) { return ecs.hasComponent<rtype::ecs::Hidden>(entity); }
    [[nodiscard]] std::string text(entity_t entity) { return ecs.getComponent<Text>(entity).text; }

    /// The sound given to the entity in the last update, taken as the SoundSystem would.
    [[nodiscard]] std::uint32_t takeSound(entity_t entity)
    {
        if (!ecs.hasComponent<SoundEffect>(entity)) {
            return 0;
        }
        const std::uint32_t sound = ecs.getComponent<SoundEffect>(entity).sound.index;
        ecs.removeComponent<SoundEffect>(entity);
        return sound;
    }

    void click()
    {
        input.mouseButtons.insert(rtype::input::MouseButton::Left);
        engine.update(0.0f);
        input.mouseButtons.clear();
        engine.update(0.0f);
    }

    /// Plays the silence and the signal up to the first letter.
    void reachTyping()
    {
        engine.update(0.0f);
        engine.update(TransmissionSystem::SilenceDuration);
        while (state().phase == TransmissionPhase::Signal) {
            engine.update(0.05f);
        }
    }
};

} // namespace

TEST_CASE("Characters of UTF-8 are counted and cut whole", "[client]")
{
    CHECK(countCharacters("") == 0);
    CHECK(countCharacters("Saturne") == 7);
    CHECK(countCharacters("Opération — Éliminer") == 20);

    CHECK(firstCharacters("Éliminer", 0).empty());
    CHECK(firstCharacters("Éliminer", 1) == "É");
    CHECK(firstCharacters("a—b", 2) == "a—");
    CHECK(firstCharacters("abc", 10) == "abc");
}

TEST_CASE("A text is wrapped between its words", "[client]")
{
    const auto measure = [](const std::string &text) { return static_cast<float>(text.size()); };

    CHECK(wrapWords("one two three four", 9.0f, measure) ==
          std::vector<std::string>{"one two", "three", "four"});
    CHECK(wrapWords("short", 100.0f, measure) == std::vector<std::string>{"short"});
    CHECK(wrapWords("unbreakable word", 4.0f, measure) == std::vector<std::string>{"unbreakable", "word"});
    CHECK(wrapWords("", 10.0f, measure) == std::vector<std::string>{""});
}

TEST_CASE("The transmission opens on a black screen, then the signal blinks with beeps", "[client]")
{
    TestTransmission test({"AB"});

    test.engine.update(0.0f);
    CHECK(test.state().phase == TransmissionPhase::Silence);
    CHECK(test.hidden(test.state().signal));
    CHECK(test.hidden(test.state().hint));

    test.engine.update(TransmissionSystem::SilenceDuration);
    CHECK(test.state().phase == TransmissionPhase::Signal);
    CHECK(test.takeSound(test.state().channel) == TestTransmission::Sounds.statics.index);
    CHECK_FALSE(test.hidden(test.state().hint));

    int beeps = 0;
    bool blinked = false;
    while (test.state().phase == TransmissionPhase::Signal) {
        test.engine.update(0.05f);
        const std::uint32_t sound = test.takeSound(test.state().signal);
        beeps += sound == TestTransmission::Sounds.beep.index ? 1 : 0;
        blinked = blinked || test.hidden(test.state().signal);
    }
    CHECK(beeps == TransmissionSystem::SignalBlinks);
    CHECK(blinked);
    CHECK(test.state().phase == TransmissionPhase::Typing);
    CHECK_FALSE(test.hidden(test.state().signal));
    CHECK_FALSE(test.hidden(test.state().channel));
}

TEST_CASE("The lines are typed letter by letter, each key with its sound", "[client]")
{
    TestTransmission test({"AB", "", "CD"});
    test.reachTyping();

    // 10 characters per second
    test.engine.update(0.1f);
    CHECK(test.text(test.lines[0]).starts_with("A"));
    CHECK(test.takeSound(test.lines[0]) == TestTransmission::Sounds.type.index);

    test.engine.update(0.1f);
    CHECK(test.text(test.lines[0]) == "AB");
    CHECK(test.takeSound(test.state().over) == TestTransmission::Sounds.line.index);
    CHECK(test.text(test.lines[2]).empty());

    for (int frame = 0; frame < 100 && test.state().phase == TransmissionPhase::Typing; ++frame) {
        test.engine.update(0.05f);
    }
    CHECK(test.text(test.lines[2]) == "CD");
    CHECK(test.state().phase == TransmissionPhase::Closing);
    CHECK(test.hidden(test.state().over));

    test.engine.update(TransmissionSystem::ClosingDelay);
    CHECK_FALSE(test.hidden(test.state().over));
    CHECK(test.takeSound(test.state().over) == TestTransmission::Sounds.end.index);

    test.engine.update(TransmissionSystem::WaitingDelay);
    CHECK(test.state().phase == TransmissionPhase::Waiting);
    CHECK(test.text(test.state().hint) == "CONTINUE");
    CHECK(test.ended == 0);
}

TEST_CASE("A press skips the transmission to its end, the next one goes on", "[client]")
{
    TestTransmission test({"A long line", "Another one"});
    test.reachTyping();
    test.engine.update(0.1f);

    test.click();
    CHECK(test.state().phase == TransmissionPhase::Closing);
    CHECK(test.text(test.lines[0]) == "A long line");
    CHECK(test.text(test.lines[1]) == "Another one");
    CHECK_FALSE(test.hidden(test.state().over));
    CHECK(test.ended == 0);

    test.click();
    CHECK(test.state().phase == TransmissionPhase::Waiting);
    CHECK(test.ended == 0);

    test.click();
    CHECK(test.state().phase == TransmissionPhase::Done);
    CHECK(test.ended == 1);

    test.click();
    CHECK(test.ended == 1);
}

TEST_CASE("Holding the mouse does not skip the transmission", "[client]")
{
    TestTransmission test({"AB"});
    test.input.mouseButtons.insert(rtype::input::MouseButton::Left);
    test.engine.update(0.0f);
    test.engine.update(0.0f);
    CHECK(test.state().phase == TransmissionPhase::Silence);
}
