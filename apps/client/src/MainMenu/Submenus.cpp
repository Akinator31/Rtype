#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <string>

namespace rtype::client
{

namespace
{

// Space between the back button and the title of a submenu
constexpr float titleSpacing = 24.0f;

/// The back button in the top left corner, then the title, centred on the same line.
void buildSubmenu(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                  const std::string &title)
{
    engine::InterfaceBuilder interface(engine.getEcs(), scene, style);
    interface.backButton([&engine] { engine.popScene(); },
                         {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    const float titleX = screenMargin + style.button.height + titleSpacing;
    const float titleY = screenMargin + ((style.button.height - style.textSize) / 2.0f);
    interface.text(title, {engine::AnchorPoint::TopLeft, titleX, titleY});
}

void createSubmenuScene(engine::Engine &engine, const engine::InterfaceStyle &style, const char *name,
                        const std::string &title)
{
    engine.createScene(name, {.overlay = true, .hidesInterfaceBelow = true},
                       [&style, title](engine::Engine &target, engine::SceneId scene) {
                           buildSubmenu(target, scene, style, title);
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
