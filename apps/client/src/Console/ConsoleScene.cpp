#include "Console/ConsoleScene.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <cstddef>
#include <string>

namespace rtype::client
{

namespace
{

constexpr float consoleHeight = 540.0f;
constexpr float margin = 24.0f;
constexpr float edgeThickness = 2.0f;
constexpr std::size_t visibleLines = 13;
constexpr const char *prompt = ">";

/// Height of the command line, padding included: the text of the field and the band behind it.
float commandLineHeight(const engine::InterfaceStyle &style)
{
    return style.console.textSize + (2.0f * style.padding);
}

/// Top of the command line, at the bottom of the console.
float commandLineTop(const engine::InterfaceStyle &style)
{
    return consoleHeight - margin - commandLineHeight(style);
}

/// The dark band over the top of the screen, with a thin line along its bottom edge.
void createBackground(engine::InterfaceBuilder &builder, float width, const engine::ConsoleStyle &look)
{
    builder.box(width, consoleHeight, look.background, {engine::AnchorPoint::TopLeft, 0.0f, 0.0f});
    builder.box(width, edgeThickness, look.edge,
                {engine::AnchorPoint::TopLeft, 0.0f, consoleHeight - edgeThickness});
}

/// The newest lines of the console, lined up with the text of the command line.
void createLines(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, const Console &console,
                 const engine::InterfaceStyle &style)
{
    const engine::Anchor anchor{engine::AnchorPoint::TopLeft, margin + style.padding, margin};
    const ecs::entity_t lines =
        builder.boundText([&console] { return console.getLastLines(visibleLines); }, anchor);
    engine::Text &text = ecs.getComponent<engine::Text>(lines);
    text.font = style.button.font;
    text.size = style.console.textSize;
    text.color = style.console.text;
}

/// The field where commands are typed, over a light band. Enter runs the command, then empties the field for
/// the next one.
void createCommandLine(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, Console &console, float width,
                       const engine::InterfaceStyle &style)
{
    const engine::Anchor anchor{engine::AnchorPoint::TopLeft, margin, commandLineTop(style)};
    builder.box(width - (2.0f * margin), commandLineHeight(style), style.console.field, anchor);

    const ecs::entity_t field = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(field, builder.scene());
    ecs.addComponent<engine::Anchor>(field, anchor);
    ecs.addComponent<ecs::Position>(field);
    ecs.addComponent<engine::Drawable>(field, engine::Layer::Interface, 1);
    ecs.addComponent<engine::Text>(field, "", style.button.font, style.console.textSize, style.console.text);
    ecs.addComponent<engine::Focusable>(field, 0);
    ecs.addComponent<engine::TextField>(
        field, prompt, input::TextInput{},
        [&ecs, &console, field](const std::string &line) {
            console.execute(line);
            ecs.getComponent<engine::TextField>(field).input.clear();
        },
        engine::TextFieldType::Console);
}

} // namespace

void buildConsole(engine::Engine &engine, engine::SceneId scene, Console &console,
                  const engine::Metrics &metrics, const engine::InterfaceStyle &style)
{
    const float width = metrics.gameSize().x;
    engine::InterfaceBuilder builder(engine.getEcs(), scene, style);
    createBackground(builder, width, style.console);
    createLines(engine.getEcs(), builder, console, style);
    createCommandLine(engine.getEcs(), builder, console, width, style);
}

} // namespace rtype::client
