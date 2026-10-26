#include "Loading/LoadingScene.hpp"

#include "Loading/LoadingScreen.hpp"
#include "MainMenu/Logo.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

namespace rtype::client
{

namespace
{

/// The bar is under the circle of the logo, its centre this far below the middle of the screen.
constexpr float barTop = 330.0f;

} // namespace

void buildLoadingScene(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                       const engine::InterfaceStyle &style, const localization::Localization &translations)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    const ecs::entity_t logo = createLogo(interface, resources);
    ecs.addComponent<engine::Opacity>(logo, 0.0f);
    const ecs::entity_t bar = interface.gauge(translations.text("loading.caption"), {},
                                              {engine::AnchorPoint::Center, 0.0f, barTop});
    ecs.getComponent<engine::Gauge>(bar).get = [&ecs, bar] {
        return ecs.getComponent<LoadingScreen>(bar).progress;
    };
    ecs.addComponent<engine::Opacity>(bar, 0.0f);
    ecs.addComponent<LoadingScreen>(bar, LoadingScreen{.logo = logo});
}

} // namespace rtype::client
