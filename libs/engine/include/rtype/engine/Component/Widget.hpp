#pragma once

#include "rtype/input/TextInput.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rtype::engine
{

/// Makes an entity a widget the player can focus, in a menu. The order sets the navigation with Up and Down.
struct Focusable {
    int order = 0;
};

/// The widget that has the focus in its scene. Set by the InterfaceSystem: at most one per scene.
struct Focused {};

/// Area of a widget the mouse can point at, centred on its position, in game units. Computed by the
/// HitboxSystem from its text.
struct Hitbox {
    /// Current size, including the Scale of the widget (a focused widget is bigger).
    float width = 0.0f;
    float height = 0.0f;
    /// Size at scale 1: layouts use it, so that widgets do not move when another one bounces.
    float baseWidth = 0.0f;
    float baseHeight = 0.0f;
};

/// A button: runs an action when it is confirmed or clicked.
struct Button {
    std::function<void()> onPress;
};

/// An on / off setting, shown as "Caption: On". Confirm and click switch it; Right turns it on, Left off.
///
/// The value is read and written through functions rather than copied, so that the widget always shows the
/// real setting, even when it is changed elsewhere (a shortcut key, for instance).
struct Toggle {
    std::string caption;
    std::function<bool()> get;
    std::function<void(bool)> set;
};

/// A value between 0 and 1, shown as a percentage: "Caption: 80%". Left, Right and the mouse wheel change it.
/// A label, written instead of the percentage, an action for Confirm and a picture after the caption are
/// optional: the volume of a player in the pause menu, muted with Confirm, shows "MUTED" and its state.
struct Slider {
    /// A picture drawn after the caption, tinted: the state of the voice of a player, for instance.
    struct Icon {
        render::TextureId texture;
        /// Part of the texture, in texture pixels.
        render::Rectangle source;
        render::Color tint = render::colors::White;
    };

    std::string caption;
    std::function<float()> get;
    std::function<void(float)> set;
    float step = 0.1f;
    /// Written instead of the percentage when it gives a text ("MUTED"), read every frame. Optional.
    std::function<std::string()> label{};
    /// What Confirm does on the slider: mute what it sets the volume of, for instance. Optional.
    std::function<void()> onConfirm{};
    /// The picture after the caption, read every frame: none when it gives nothing. Optional.
    std::function<std::optional<Icon>()> icon{};
    /// Whether it cannot be changed now, read every frame: drawn dimmed, its value in red, and skipped by
    /// the focus and the pointer. The volume of a player muted by an administrator, for instance. Optional.
    std::function<bool()> locked{};
};

/// Draws a Slider as a rail with a handle (InterfaceStyle::slider) instead of a text, and lets the mouse drag
/// it. The Text of the entity gives the font of its caption. Written by the InterfaceSystem.
struct SliderTrack {
    /// 0 for the look at rest, 1 while the pointer is over the handle, in between while it changes.
    float hover = 0.0f;
    /// 0 for the look at rest, 1 while the player drags the handle, in between while it changes.
    float grab = 0.0f;
    /// 0 for the look at rest, 1 while the slider is under the pointer or has the focus: its caption lights
    /// up.
    float focus = 0.0f;
    /// Whether the slider is under the pointer or has the focus, to sound when it lights up.
    bool lit = false;
    /// Width of the row, in game units, or 0 for the width of the style.
    float width = 0.0f;
};

/// Draws a Toggle as "OFF - dot - ON" (InterfaceStyle::toggle) instead of a text. The Text of the entity
/// gives the font of its caption. Written by the InterfaceSystem.
struct SwitchTrack {
    /// 0 for the look at rest, 1 while the pointer is over the switch, in between while it changes.
    float hover = 0.0f;
    /// 0 for the look at rest, 1 while the toggle is under the pointer or has the focus: its caption lights
    /// up.
    float focus = 0.0f;
    /// Whether the toggle is under the pointer or has the focus, to sound when it lights up.
    bool lit = false;
    /// Place of the dot: 0 on the OFF side, 1 on the ON side, in between while it moves.
    float position = 0.0f;
};

/// A value between 0 and 1 shown as a rail lit up to its value (InterfaceStyle::gauge): a life, a charge, a
/// progress. It shows, but cannot be changed: it does not get the focus. The value is read through a
/// function, so that the gauge always shows the real one. Without a caption, the rail is on the position of
/// the entity: a health bar above a ship.
struct Gauge {
    std::string caption;
    std::function<float()> get;
    /// Length of the rail, in game units, or 0 for the width of the style.
    float width = 0.0f;
    /// At or below this value, the light turns red (GaugeStyle::lowLight): a life about to run out. 0 for a
    /// light that stays white.
    float low = 0.0f;
    /// Whether the value is written as a percentage on the right of the caption.
    bool showsPercentage = true;
};

/// One option among several, shown as "Caption: Option". Left, Right, the mouse wheel and a click go through
/// the options, and around.
struct Choice {
    std::string caption;
    std::vector<std::string> options;
    /// Returns the index of the current option.
    std::function<std::size_t()> get;
    std::function<void(std::size_t)> set;
};

/// How a text field looks and behaves when it has the focus.
enum class TextFieldType {
    /// A widget of a menu, shown as "Caption: content": framed and bigger while it has the focus, like the
    /// other widgets.
    Menu,
    /// A command line, shown as "Caption content", the caption being the prompt: it keeps its size and has no
    /// frame, its prompt and cursor take the colours of InterfaceStyle::console.
    Console,
    /// A field of a form (InterfaceStyle::field): its caption above, its value, and a rule under it that
    /// lights
    /// up as a line of light with the focus. Its error, if any, is written under the rule, in red. It keeps
    /// its size.
    Line,
    /// A code of digits typed in cells (InterfaceStyle::field), one per character, as many as the maximum
    /// length of its input: the cell being typed lights up. It is submitted as soon as it is full.
    Code,
};

/// A text field, edited while it has the focus, with everything a TextInput offers (cursor, selection,
/// clipboard); Confirm submits it. Without caption, it shows only its content.
struct TextField {
    std::string caption{};
    input::TextInput input{};
    /// Called with the content when the player confirms the field. Can be empty.
    std::function<void(const std::string &)> onSubmit{};
    TextFieldType type = TextFieldType::Menu;
    /// A Line or Code field: what is wrong with its content, shown under it in red, or empty. Set by the
    /// owner of the scene; cleared when the player changes the content.
    std::string error{};
    /// A Line or Code field: 0 at rest, 1 while it has the focus, in between while it lights up. Written by
    /// the InterfaceSystem.
    float light = 0.0f;
};

/// Whether a field is drawn as a field of a form (Line or Code) rather than as a text.
[[nodiscard]] bool isFormField(const TextField &field);

/// How the ring of an Avatar is drawn.
enum class AvatarKind {
    /// An account: a silver ring.
    Account,
    /// An account signed in with Discord: a bright ring with a light around it.
    Discord,
    /// A guest: a dotted ring, in the grey of what is disabled.
    Guest,
};

/// The round picture of a player (InterfaceStyle::avatar): their picture if they have one, else their
/// initial, in a ring.
struct Avatar {
    /// A square texture, already round (render::ResourceManager::loadRoundPicture()), of `pictureSize`
    /// pixels.
    std::optional<render::TextureId> picture{};
    float pictureSize = 128.0f;
    /// Shown without a picture: the first letter of the name, or "?" for a guest.
    std::string initial{};
    AvatarKind kind = AvatarKind::Account;
    /// Diameter of the ring, in game units.
    float size = 64.0f;
};

/// A row of a list (InterfaceStyle::list): a title in capitals, a line of detail under it, a text on its
/// right, an avatar on its left if any, and a rule under it. Its Text is the title; with a Panel and a
/// Button, the row is pressed as a button and lit as one: the rule becomes a line of light with a star at its
/// end. Without them, the row is disabled, drawn dim.
struct ListRow {
    std::string detail{};
    std::string side{};
    float width = 0.0f;
    std::optional<Avatar> avatar{};
};

/// A row of a page of settings (InterfaceStyle::setting): its caption (the Text of the entity) on the left,
/// its value on the right, on the column shared by every row, and a rule under it. With a Panel, it lights up
/// as the other widgets: a veil from its left, a mark on its left edge, a line of light along its bottom.
///
/// With `change`, it is a choice: Left, Right, the mouse wheel, Confirm and a click go through its values,
/// and chevrons appear around the value while it is lit. Without it, with a Button, it opens something: a
/// chevron on its right, and no value.
struct SettingRow {
    /// The value shown, read every frame so that the row always shows the real setting. Can be empty.
    std::function<std::string()> value{};
    /// Goes to the next value (+1) or the previous one (-1). Empty for a row that opens something.
    std::function<void(int)> change{};
    /// Width of the row, in game units, or 0 for the width of the style.
    float width = 0.0f;
};

/// How a key of a KeyRow is drawn.
enum class KeyCapState {
    /// A key bound to the action.
    Normal,
    /// The key just chosen for the action, lit.
    New,
    /// The empty key while the player is asked for one, in dashes.
    Waiting,
    /// A key that another action is about to take, in the red of the alerts.
    Conflict,
};

/// A key of a KeyRow: its name as it is printed on the keyboard, and its state.
struct KeyCap {
    std::string label{};
    KeyCapState state = KeyCapState::Normal;
};

/// A row of the table of the controls (InterfaceStyle::keys): the action (the Text of the entity), its keys,
/// each in a frame like a key of a keyboard, and the buttons of the gamepad in pills. With a Panel and a
/// Button, it lights up as a SettingRow and is pressed as a button. Everything is read every frame.
struct KeyRow {
    std::function<std::vector<KeyCap>()> keys{};
    std::function<std::vector<std::string>()> buttons{};
    /// Whether the row is drawn dim, while the player chooses a key for another row. Can be empty.
    std::function<bool()> dimmed{};
    /// Width of the row, and where its keys and its buttons start from its left edge, in game units.
    float width = 0.0f;
    float keysColumn = 0.0f;
    float buttonsColumn = 0.0f;
};

/// The name of a group of rows, in small capitals (the Text of the entity), followed by a fine rule up to the
/// end of the group (InterfaceStyle::setting). Without width, the name alone: the head of a column.
struct SectionLabel {
    /// From the left of the name to the end of the rule, in game units, or 0 for no rule.
    float width = 0.0f;
};

/// Label of a widget that shows a value: "Caption: value".
[[nodiscard]] std::string widgetLabel(const Toggle &toggle);
[[nodiscard]] std::string widgetLabel(const Slider &slider);
[[nodiscard]] std::string widgetLabel(const Choice &choice);
[[nodiscard]] std::string widgetLabel(const TextField &field);
/// Text before the content of a text field: "Caption: " for a menu field, "Caption " for a console field, or
/// nothing without caption.
[[nodiscard]] std::string fieldPrefix(const TextField &field);
/// The content of a text field as it is shown, up to a byte offset of its text (all of it by default): the
/// text, or one `*` per character for a secret field (TextInput::setSecret()), so that a password is never
/// drawn.
[[nodiscard]] std::string fieldContent(const TextField &field, std::size_t bytes = std::string::npos);

} // namespace rtype::engine
