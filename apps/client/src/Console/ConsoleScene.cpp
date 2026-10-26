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
constexpr std::size_t visibleLines = 11;
constexpr const char *prompt = ">";

// Drawing order inside the interface layer: over every menu (the flare of the logo is over its menu) and
// under the toasts. Entities are recycled, so their ids say nothing about the order they were created in:
// without these, the veil could be drawn over the text.
constexpr int veilOrder = 900;
constexpr int lineOrder = 901;
constexpr int textOrder = 902;

/// A box of the console, drawn at this order.
void createBox(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, float width, float height,
               render::Color color, engine::Anchor anchor, int order)
{
    const ecs::entity_t box = builder.box(width, height, color, anchor);
    ecs.getComponent<engine::Drawable>(box).order = order;
}

/// Height of the command line: the text of the field.
float commandLineHeight(const engine::InterfaceStyle &style)
{
    return style.console.textSize + style.padding;
}

/// Top of the command line, at the bottom of the console.
float commandLineTop(const engine::InterfaceStyle &style)
{
    return consoleHeight - style.console.margin - commandLineHeight(style);
}

/// Top of the rule above the command line.
float ruleTop(const engine::InterfaceStyle &style)
{
    return commandLineTop(style) - style.console.ruleGap - style.console.ruleThickness;
}

/// The dark veil over the top of the screen, with a fine line of light along its bottom edge and a fainter
/// one around it, like the lines of the buttons.
void createBackground(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, float width,
                      const engine::ConsoleStyle &look)
{
    createBox(ecs, builder, width, consoleHeight, look.background,
              {engine::AnchorPoint::TopLeft, 0.0f, 0.0f}, veilOrder);
    createBox(ecs, builder, width, look.edgeGlowThickness, look.edgeGlow,
              {engine::AnchorPoint::TopLeft, 0.0f,
               consoleHeight - ((look.edgeGlowThickness + look.edgeThickness) / 2.0f)},
              lineOrder);
    createBox(ecs, builder, width, look.edgeThickness, look.edge,
              {engine::AnchorPoint::TopLeft, 0.0f, consoleHeight - look.edgeThickness}, lineOrder);
}

/// The newest lines of the console, lined up with the text of the command line.
void createLines(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, const Console &console,
                 const engine::InterfaceStyle &style)
{
    const engine::Anchor anchor{engine::AnchorPoint::TopLeft, style.console.margin, style.console.margin};
    const ecs::entity_t lines =
        builder.boundText([&console] { return console.getLastLines(visibleLines); }, anchor);
    engine::Text &text = ecs.getComponent<engine::Text>(lines);
    text.font = style.button.font;
    text.size = style.console.textSize;
    text.color = style.console.text;
    ecs.getComponent<engine::Drawable>(lines).order = textOrder;
}

/// The field where commands are typed, under a fine rule. Enter runs the command, then empties the field for
/// the next one.
void createCommandLine(ecs::Ecs &ecs, engine::InterfaceBuilder &builder, Console &console, float width,
                       const engine::InterfaceStyle &style)
{
    const float margin = style.console.margin;
    const engine::Anchor anchor{engine::AnchorPoint::TopLeft, margin, commandLineTop(style)};
    createBox(ecs, builder, width - (2.0f * margin), style.console.ruleThickness, style.console.rule,
              {engine::AnchorPoint::TopLeft, margin, ruleTop(style)}, lineOrder);

    const ecs::entity_t field = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(field, builder.scene());
    ecs.addComponent<engine::Anchor>(field, anchor);
    ecs.addComponent<ecs::Position>(field);
    ecs.addComponent<engine::Drawable>(field, engine::Layer::Interface, textOrder);
    ecs.addComponent<engine::Text>(field, "", style.button.font, style.console.textSize,
                                   style.console.command);
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
    const float width = metrics.visibleArea().width;
    engine::InterfaceBuilder builder(engine.getEcs(), scene, style);
    createBackground(engine.getEcs(), builder, width, style.console);
    createLines(engine.getEcs(), builder, console, style);
    createCommandLine(engine.getEcs(), builder, console, width, style);
}

} // namespace rtype::client
