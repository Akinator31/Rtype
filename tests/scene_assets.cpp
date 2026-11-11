#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Scene/SceneAssetLoader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <stdexcept>
#include <string>

namespace
{

using namespace rtype::engine;

struct AssetState {
    bool ready = false;
    bool failed = false;
    int requests = 0;
    int checked = 0;
    int unloaded = 0;
};

class FakeAssets final : public SceneAssetLoader
{
public:
    std::map<SceneAsset, AssetState> states;

    SceneAssetRequest requestAsset(const SceneAsset &asset) override
    {
        AssetState &state = states[asset];
        ++state.requests;
        return {
            [&state] { return state.ready; },
            [&state] {
                if (state.failed) {
                    throw std::runtime_error("missing asset");
                }
                ++state.checked;
            },
            [&state] { ++state.unloaded; },
        };
    }
};

void configure(Engine &engine, FakeAssets &assets)
{
    engine.createScene("loading");
    engine.setSceneAssetLoader(assets, "loading");
}

} // namespace

TEST_CASE("Scene assets load automatically without blocking the scene update", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    engine.createScene("game", {.assets = {ship}});

    engine.switchScene("game");
    engine.update(0.016f);
    CHECK(engine.getSceneLoadState() == SceneLoadState::Loading);
    CHECK(engine.isSceneOpen("loading"));
    CHECK_FALSE(engine.isSceneOpen("game"));
    CHECK(assets.states[ship].requests == 1);

    assets.states[ship].ready = true;
    engine.update(0.016f);
    CHECK(engine.getSceneLoadState() == SceneLoadState::Idle);
    CHECK_FALSE(engine.isSceneOpen("loading"));
    CHECK(engine.isSceneOpen("game"));
    CHECK(assets.states[ship].checked == 1);

    engine.createScene("menu");
    engine.switchScene("menu");
    engine.update(0.016f);
    CHECK(assets.states[ship].unloaded == 1);
}

TEST_CASE("Ready scene assets open without a loading frame", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    assets.states[ship].ready = true;
    engine.createScene("game", {.assets = {ship, ship}});
    engine.switchScene("game");
    engine.update(0.016f);
    CHECK(engine.isSceneOpen("game"));
    CHECK_FALSE(engine.isSceneOpen("loading"));
    CHECK(assets.states[ship].requests == 1);
}

TEST_CASE("Open scenes share assets until their last use closes", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    assets.states[ship].ready = true;
    engine.createScene("game", {.assets = {ship}});
    engine.createScene("pause", {.assets = {ship}});
    engine.switchScene("game");
    engine.update(0.016f);
    engine.pushScene("pause");
    engine.update(0.016f);
    CHECK(assets.states[ship].requests == 1);
    engine.popScene();
    engine.update(0.016f);
    CHECK(assets.states[ship].unloaded == 0);
    engine.popScene();
    engine.update(0.016f);
    CHECK(assets.states[ship].unloaded == 1);
}

TEST_CASE("A failed scene asset can be retried", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    assets.states[ship].ready = true;
    assets.states[ship].failed = true;
    engine.createScene("game", {.assets = {ship}});
    engine.switchScene("game");
    engine.update(0.016f);
    CHECK(engine.getSceneLoadState() == SceneLoadState::Failed);
    CHECK(engine.getSceneLoadError() == "missing asset");
    CHECK(engine.isSceneOpen("loading"));
    CHECK_FALSE(engine.isSceneOpen("game"));

    assets.states[ship].failed = false;
    engine.retrySceneLoad();
    engine.update(0.016f);
    CHECK(engine.getSceneLoadState() == SceneLoadState::Idle);
    CHECK(engine.isSceneOpen("game"));
    CHECK(assets.states[ship].requests == 2);
}

TEST_CASE("A later scene request cancels the pending scene", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    engine.createScene("game", {.assets = {ship}});
    engine.createScene("menu");
    engine.switchScene("game");
    engine.update(0.016f);
    engine.switchScene("menu");
    engine.update(0.016f);
    CHECK(engine.getSceneLoadState() == SceneLoadState::Idle);
    CHECK(engine.isSceneOpen("menu"));
    CHECK_FALSE(engine.isSceneOpen("loading"));
    CHECK_FALSE(engine.isSceneOpen("game"));

    assets.states[ship].ready = true;
    engine.update(0.016f);
    CHECK(assets.states[ship].unloaded == 1);
    CHECK_FALSE(engine.isSceneOpen("game"));
}

TEST_CASE("A scene keeping its assets opens again without loading them", "[engine][assets]")
{
    Engine engine;
    FakeAssets assets;
    configure(engine, assets);
    const SceneAsset planet{AssetType::Model, "planet.glb"};
    const SceneAsset ship{AssetType::Model, "ship.glb"};
    assets.states[planet].ready = true;
    assets.states[ship].ready = true;
    engine.createScene("menu", {.assets = {planet}, .keepsAssets = true});
    engine.createScene("game", {.assets = {ship}});

    engine.switchScene("menu");
    engine.update(0.016f);
    engine.switchScene("game");
    engine.update(0.016f);
    CHECK(assets.states[planet].unloaded == 0);

    engine.switchScene("menu");
    engine.update(0.016f);
    CHECK(engine.isSceneOpen("menu"));
    CHECK_FALSE(engine.isSceneOpen("loading"));
    CHECK(assets.states[planet].requests == 1);
    CHECK(assets.states[planet].unloaded == 0);
    CHECK(assets.states[ship].unloaded == 1);

    engine.unloadSceneAssets();
    CHECK(assets.states[planet].unloaded == 1);
}
