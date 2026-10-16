#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <string>

namespace rtype::client
{

namespace
{

void createSubmenuScene(engine::Engine &engine, const engine::InterfaceStyle &style, const char *name,
                        const std::string &title)
{
    engine.createScene(name, {.overlay = true, .hidesInterfaceBelow = true},
                       [&style, title](engine::Engine &target, engine::SceneId scene) {
                           engine::InterfaceBuilder interface(target.getEcs(), scene, style);
                           interface.header(title, [&target] { target.popScene(); },
                                            {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
                       });
}

} // namespace

void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style)
{
    createSubmenuScene(engine, style, PlayMenuScene, "Play");
    createSubmenuScene(engine, style, OnlineMenuScene, "Online");
    createSubmenuScene(engine, style, SettingsMenuScene, "Settings");
}

} // namespace rtype::client
