#include "rtype/engine/Interface/MenuBuilder.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"

#include <utility>

namespace rtype::engine
{

namespace
{

// Drawing orders inside the interface layer
constexpr int backgroundOrder = 0;
constexpr int contentOrder = 1;

// Vertical positions, as parts of the game height
constexpr float titleHeight = 0.18f;
constexpr float columnHeight = 0.56f;

} // namespace

MenuBuilder::MenuBuilder(ecs::Ecs &ecs, Scenes &scenes, const Metrics &metrics, const InterfaceStyle &style,
                         const std::string &title, SceneSettings settings)
    : MenuBuilder(ecs, scenes.open(settings), metrics, style, title, settings.overlay)
{
}

MenuBuilder::MenuBuilder(ecs::Ecs &ecs, SceneId scene, const Metrics &metrics, const InterfaceStyle &style,
                         const std::string &title, bool overlay)
    : _ecs(ecs), _style(style), _scene(scene), _column(ecs.createEntity())
{
    const render::Vector2 game = metrics.gameSize();
    const render::Rectangle screen = metrics.visibleArea();

    // A background covering the screen: dims the game behind an overlay, hides it otherwise
    const ecs::entity_t background = ecs.createEntity();
    ecs.addComponent<SceneMember>(background, _scene);
    ecs.addComponent<ecs::Position>(background, screen.x + (screen.width / 2.0f),
                                    screen.y + (screen.height / 2.0f));
    ecs.addComponent<Drawable>(background, Layer::Interface, backgroundOrder);
    ecs.addComponent<Box>(background, screen.width, screen.height, overlay ? style.dim : style.background,
                          0.0f);

    if (!title.empty()) {
        const ecs::entity_t heading = ecs.createEntity();
        ecs.addComponent<SceneMember>(heading, _scene);
        ecs.addComponent<ecs::Position>(heading, game.x / 2.0f, game.y * titleHeight);
        ecs.addComponent<Drawable>(heading, Layer::Interface, contentOrder);
        ecs.addComponent<Text>(heading, title, style.title.font, style.titleSize, style.text);
        ecs.addComponent<Title>(heading);
    }

    ecs.addComponent<SceneMember>(_column, _scene);
    ecs.addComponent<ecs::Position>(_column, game.x / 2.0f, game.y * columnHeight);
    ecs.addComponent<Column>(_column, style.spacing);
}

ecs::entity_t MenuBuilder::widget(const std::string &text, bool focusable)
{
    const ecs::entity_t entity = _ecs.createEntity();
    _ecs.addComponent<SceneMember>(entity, _scene);
    ecs::attach(_ecs, entity, _column);
    _ecs.addComponent<ColumnItem>(entity, _count);
    if (focusable) {
        _ecs.addComponent<Focusable>(entity, _count);
    }
    ++_count;
    _ecs.addComponent<Drawable>(entity, Layer::Interface, contentOrder);
    _ecs.addComponent<Text>(entity, text, _style.font, _style.textSize, _style.text);
    _ecs.addComponent<Scale>(entity);
    return entity;
}

ecs::entity_t MenuBuilder::label(const std::string &text)
{
    return widget(text, false);
}

ecs::entity_t MenuBuilder::button(const std::string &text, std::function<void()> onPress)
{
    const ecs::entity_t entity = widget(text, true);
    _ecs.addComponent<Text>(entity, text, _style.flare.font, _style.flare.textSize, _style.flare.text);
    _ecs.addComponent<Panel>(entity, _style.flare.height);
    _ecs.addComponent<Button>(entity, std::move(onPress));
    return entity;
}

ecs::entity_t MenuBuilder::toggle(const std::string &caption, std::function<bool()> get,
                                  std::function<void(bool)> set)
{
    Toggle toggle{caption, std::move(get), std::move(set)};
    const ecs::entity_t entity = widget(widgetLabel(toggle), true);
    _ecs.addComponent<Toggle>(entity, std::move(toggle));
    return entity;
}

ecs::entity_t MenuBuilder::slider(const std::string &caption, std::function<float()> get,
                                  std::function<void(float)> set, float step)
{
    Slider slider{caption, std::move(get), std::move(set), step};
    const ecs::entity_t entity = widget(widgetLabel(slider), true);
    _ecs.addComponent<Slider>(entity, std::move(slider));
    return entity;
}

ecs::entity_t MenuBuilder::choice(const std::string &caption, std::vector<std::string> options,
                                  std::function<std::size_t()> get, std::function<void(std::size_t)> set)
{
    Choice choice{caption, std::move(options), std::move(get), std::move(set)};
    const ecs::entity_t entity = widget(widgetLabel(choice), true);
    _ecs.addComponent<Choice>(entity, std::move(choice));
    return entity;
}

ecs::entity_t MenuBuilder::textField(const std::string &caption, const std::string &content,
                                     std::function<void(const std::string &)> onSubmit, std::size_t maxLength)
{
    TextField field{.caption = caption, .onSubmit = std::move(onSubmit)};
    field.input.setText(content);
    field.input.setMaxLength(maxLength);
    const ecs::entity_t entity = widget(widgetLabel(field), true);
    _ecs.addComponent<TextField>(entity, std::move(field));
    return entity;
}

ecs::entity_t MenuBuilder::passwordField(const std::string &caption, std::size_t maxLength)
{
    TextField field{.caption = caption};
    field.input.setSecret(true);
    field.input.setMaxLength(maxLength);
    const ecs::entity_t entity = widget(widgetLabel(field), true);
    _ecs.addComponent<TextField>(entity, std::move(field));
    return entity;
}

} // namespace rtype::engine
