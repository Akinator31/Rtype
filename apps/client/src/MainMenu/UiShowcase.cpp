#include "MainMenu/UiShowcase.hpp"

#include "MainMenu/Layout.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace rtype::client
{

namespace
{

/// The left column, from the top of the screen: the sliders, then the switches, then the gauges.
constexpr float slidersTop = 230.0f;
constexpr float slidersSpacing = 96.0f;
constexpr float switchesTop = 540.0f;
constexpr float switchesSpacing = 64.0f;
constexpr float gaugesTop = 770.0f;
constexpr float gaugesSpacing = 70.0f;

/// The titles are in a column of their own, in the middle of the screen.
constexpr float titlesLeft = 820.0f;
constexpr float titlesTop = 520.0f;
constexpr float titlesSpacing = 160.0f;

/// The buttons of the notifications are in a column on the right, the last one at buttonsBottom from the
/// bottom.
constexpr float buttonsBottom = 150.0f;
constexpr float buttonsSpacing = 76.0f;

/// A setting that only the widgets of the scene read and write.
template <typename Value> using Setting = std::shared_ptr<Value>;

float rowTop(float top, float spacing, int row)
{
    return top + (static_cast<float>(row) * spacing);
}

void createSliders(engine::InterfaceBuilder &interface, const engine::InterfaceStyle &style)
{
    // The area of a slider goes beyond its rail by the handle: the rail starts at the margin, like the others
    const float railOverhang = style.slider.handleDragRadius;
    constexpr std::array<std::pair<const char *, float>, 3> sliders{{{"VOLUME", 0.65f}, {"MUSIC", 0.25f},
                                                                      {"EFFECTS", 1.0f}}};
    int row = 0;
    for (const auto &[caption, value] : sliders) {
        const auto setting = std::make_shared<float>(value);
        interface.slider(
            caption, [setting] { return *setting; }, [setting](float newValue) { *setting = newValue; }, 0.05f,
            {engine::AnchorPoint::TopLeft, screenMargin - railOverhang,
             rowTop(slidersTop, slidersSpacing, row++)});
    }
}

void createSwitches(engine::InterfaceBuilder &interface)
{
    constexpr std::array<std::pair<const char *, bool>, 3> switches{{{"FULLSCREEN", false}, {"SUBTITLES", true},
                                                                      {"VIBRATION", false}}};
    int row = 0;
    for (const auto &[caption, value] : switches) {
        const auto setting = std::make_shared<bool>(value);
        interface.toggle(
            caption, [setting] { return *setting; }, [setting](bool newValue) { *setting = newValue; },
            {engine::AnchorPoint::TopLeft, screenMargin, rowTop(switchesTop, switchesSpacing, row++)});
    }
}

/// Three gauges, a quarter, two thirds, and full.
void createGauges(engine::InterfaceBuilder &interface)
{
    constexpr std::array<std::pair<const char *, float>, 3> gauges{{{"SHIELD", 0.25f}, {"ENERGY", 0.65f},
                                                                     {"CHARGE", 1.0f}}};
    int row = 0;
    for (const auto &[caption, value] : gauges) {
        interface.gauge(
            caption, [value] { return value; },
            {engine::AnchorPoint::TopLeft, screenMargin, rowTop(gaugesTop, gaugesSpacing, row++)});
    }
}

/// Two titles, as a screen title and as a part of a screen.
void createTitles(ecs::Ecs &ecs, engine::InterfaceBuilder &interface, const engine::InterfaceStyle &style)
{
    const ecs::entity_t large =
        interface.title("R-TYPE", {engine::AnchorPoint::TopLeft, titlesLeft, titlesTop});
    ecs.getComponent<engine::Text>(large).size = style.titleSize;
    interface.title("Sector 9", {engine::AnchorPoint::TopLeft, titlesLeft, titlesTop + titlesSpacing});
}

/// A flare button on the right, `rowsBelow` rows above the bottom one.
void createNotificationButton(ecs::Ecs &ecs, engine::InterfaceBuilder &interface,
                              localization::Localization &translations, const std::string &key,
                              std::function<void()> onPress, int rowsBelow)
{
    const ecs::entity_t button = interface.flareButton(
        translations.text(key), std::move(onPress),
        {engine::AnchorPoint::BottomRight, -screenMargin,
         -buttonsBottom - (static_cast<float>(rowsBelow) * buttonsSpacing)});
    ecs.addComponent<engine::BoundText>(button, [&translations, key] { return translations.text(key); });
}

/// The three types of notifications, by their key in the translations.
constexpr std::array<std::pair<const char *, engine::ToastType>, 3> notificationTypes{{
    {"info", engine::ToastType::Info},
    {"bonus", engine::ToastType::Success},
    {"alert", engine::ToastType::Error},
}};

/// The buttons that show the notifications: the lines of the events, the announcements, the banners, one
/// type after the other at each press, and all of them together.
void createNotificationButtons(engine::Engine &engine, engine::InterfaceBuilder &interface,
                               localization::Localization &translations)
{
    ecs::Ecs &ecs = engine.getEcs();
    const auto events = [&engine, &translations] {
        engine.showToast("VORTEX", translations.text("showcase.joined"), "", engine::ToastType::Info);
        engine.showToast("ORBITAL MODULE", translations.text("showcase.taken"), "KAZE",
                         engine::ToastType::Success);
        engine.showToast("SHIELD", translations.text("showcase.critical"), "P1", engine::ToastType::Error);
    };
    const auto announcements = [&engine, &translations] {
        for (const auto &[name, type] : notificationTypes) {
            const std::string key = std::string("showcase.announcement.") + name;
            engine.showAnnouncement(translations.text(key + ".title"), translations.text(key + ".detail"), type);
        }
    };
    // A banner at a time, so that each press shows the next type
    const auto next = std::make_shared<std::size_t>(0);
    const auto banner = [&engine, &translations, next] {
        const auto &[name, type] = notificationTypes[*next % notificationTypes.size()];
        ++*next;
        const std::string key = std::string("showcase.banner.") + name;
        engine.showBanner(translations.text(key + ".title"), translations.text(key + ".detail"), type);
    };
    createNotificationButton(ecs, interface, translations, "showcase.all",
                             [events, announcements, banner] {
                                 events();
                                 announcements();
                                 banner();
                             },
                             0);
    createNotificationButton(ecs, interface, translations, "showcase.banner", banner, 1);
    createNotificationButton(ecs, interface, translations, "showcase.announcements", announcements, 2);
    createNotificationButton(ecs, interface, translations, "showcase.events", events, 3);
}

} // namespace

void createUiShowcaseScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                           localization::Localization &translations)
{
    engine.createScene(
        UiShowcaseScene, {.overlay = true, .hidesInterfaceBelow = true},
        [&style, &translations](engine::Engine &target, engine::SceneId scene) {
            engine::InterfaceBuilder interface(target.getEcs(), scene, style);
            interface.backButton([&target] { target.popScene(); },
                                 {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
            interface.boundTitle([&translations] { return translations.text("showcase.title"); },
                                 {engine::AnchorPoint::TopLeft,
                                  screenMargin + style.button.height + style.headerSpacing,
                                  screenMargin + ((style.button.height - style.textSize) / 2.0f)});
            createSliders(interface, style);
            createSwitches(interface);
            createGauges(interface);
            createTitles(target.getEcs(), interface, style);
            createNotificationButtons(target, interface, translations);
        });
}

} // namespace rtype::client
