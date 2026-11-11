#include "Online/OnlineScreen.hpp"

#include "MainMenu/Layout.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <utility>

namespace rtype::client
{

namespace
{

/// The colours of the tones, in the order of OnlineScreen::Tone: each one readable on the black of the sky, a
/// hint too, at its small size.
constexpr render::Color bright{255, 255, 255, 255};
constexpr render::Color detail{195, 200, 205, 255};
constexpr render::Color hint{154, 160, 166, 255};
constexpr render::Color alert{232, 114, 110, 255};

/// The logo of Discord before a button: its size, and the space between it and the text of the button.
constexpr float logoSize = 34.0f;
constexpr float logoGap = 10.0f;

/// Big enough to cover any screen, whatever its shape.
constexpr float veilSize = 8000.0f;
constexpr render::Color veilColor{0, 0, 0, 170};

} // namespace

std::string initialOf(const std::string &name)
{
    if (name.empty()) {
        return "?";
    }
    // The bytes that continue a character of UTF-8 start with the bits 10
    std::size_t length = 1;
    while (length < name.size() && (static_cast<unsigned char>(name[length]) & 0xC0U) == 0x80U) {
        ++length;
    }
    return toCapitals(name.substr(0, length));
}

OnlineScreen::OnlineScreen(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style)
    : _ecs(engine.getEcs()), _style(style), _ui(engine.getEcs(), scene, style)
{
}

void OnlineScreen::header(const std::string &title, std::function<void()> onBack)
{
    _ui.header(toCapitals(title), std::move(onBack),
               {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
}

void OnlineScreen::veil()
{
    const ecs::entity_t veil =
        _ui.box(veilSize, veilSize, veilColor, {engine::AnchorPoint::Center, 0.0f, 0.0f});
    // Under everything else of the screen, whatever the order the entities are made in
    _ecs.getComponent<engine::Drawable>(veil).order = 0;
}

ecs::entity_t OnlineScreen::note(const std::string &text, engine::Anchor anchor, Tone tone, float size)
{
    const ecs::entity_t entity = _ui.text(text, anchor);
    engine::Text &shown = _ecs.getComponent<engine::Text>(entity);
    shown.font = _style.field.captionFont;
    shown.size = size;
    constexpr render::Color colors[] = {bright, detail, hint, alert};
    shown.color = colors[static_cast<int>(tone)];
    return entity;
}

ecs::entity_t OnlineScreen::field(const std::string &caption, const std::string &content, int row,
                                  std::size_t maxLength, std::function<void(const std::string &)> onSubmit)
{
    const float top = online_layout::fieldsTop + (static_cast<float>(row) * online_layout::fieldsSpacing);
    return _ui.textField(toCapitals(caption), content,
                         {engine::AnchorPoint::TopLeft, online_layout::columnLeft, top}, maxLength,
                         std::move(onSubmit));
}

ecs::entity_t OnlineScreen::password(const std::string &caption, int row, std::size_t maxLength,
                                     std::function<void(const std::string &)> onSubmit)
{
    const float top = online_layout::fieldsTop + (static_cast<float>(row) * online_layout::fieldsSpacing);
    return _ui.passwordField(toCapitals(caption),
                             {engine::AnchorPoint::TopLeft, online_layout::columnLeft, top}, maxLength,
                             std::move(onSubmit));
}

ecs::entity_t OnlineScreen::button(const std::string &text, std::function<void()> onPress,
                                   engine::Anchor anchor)
{
    return _ui.button(toCapitals(text), std::move(onPress), anchor);
}

ecs::entity_t OnlineScreen::dangerButton(const std::string &text, std::function<void()> onPress,
                                         engine::Anchor anchor)
{
    return _ui.dangerButton(toCapitals(text), std::move(onPress), anchor);
}

ecs::entity_t OnlineScreen::discordButton(const std::string &text, std::function<void()> onPress,
                                          engine::Anchor anchor, render::TextureId logo)
{
    // The logo sits where the text of a button would start, and the button moves right by its size
    const float middle = anchor.y + (_style.flare.height / 2.0f) - (logoSize / 2.0f);
    const ecs::entity_t sprite = _ui.sprite(logo, {0.0f, 0.0f, 128.0f, 128.0f}, logoSize, logoSize,
                                            {anchor.point, anchor.x + _style.flare.padding, middle});
    _ecs.getComponent<engine::Drawable>(sprite).order = 2;
    return button(text, std::move(onPress), {anchor.point, anchor.x + logoSize + logoGap, anchor.y});
}

void OnlineScreen::divider(float top, float bottom, const std::string &text)
{
    constexpr float gap = 30.0f;
    constexpr render::Color line{51, 55, 59, 255};
    const float middle = (top + bottom) / 2.0f;
    _ui.box(1.5f, middle - gap - top, line, {engine::AnchorPoint::TopLeft, online_layout::dividerX, top});
    _ui.box(1.5f, bottom - middle - gap, line,
            {engine::AnchorPoint::TopLeft, online_layout::dividerX, middle + gap});
    const ecs::entity_t label =
        note(toCapitals(text), {engine::AnchorPoint::Left, online_layout::dividerX - 960.0f, middle - 540.0f},
             Tone::Hint, 20.0f);
    // Centred on the line
    _ecs.getComponent<engine::Anchor>(label) = {engine::AnchorPoint::Center, online_layout::dividerX - 960.0f,
                                                middle - 540.0f};
}

void OnlineScreen::rule(engine::Anchor anchor, float width, render::Color color)
{
    _ui.box(width, 1.5f, color, anchor);
}

ecs::entity_t OnlineScreen::question(const std::string &text, float top)
{
    const ecs::entity_t title = _ui.title(toCapitals(text), {engine::AnchorPoint::Top, 0.0f, top});
    _ecs.getComponent<engine::Text>(title).size = 44.0f;
    return title;
}

} // namespace rtype::client
