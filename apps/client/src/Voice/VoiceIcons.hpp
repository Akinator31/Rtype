#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtype::client
{

/// The icon beside the name of a pilot in the HUD and in the pause menu, from the Figma design (Série 16,
/// `Jeu · Icône de voix`): what its voice does now. A pilot that talks lights up its name too.
enum class VoiceIcon : std::uint8_t {
    /// Nothing to tell: silent, or no voice at all.
    None,
    /// It talks: its voice is heard now. For the player of this client: its voice goes out.
    Speaking,
    /// Muted by itself.
    Muted,
    /// Muted by the player of this client, for this client only.
    MutedByMe,
    /// No microphone: it listens only.
    NoMicrophone,
    /// Deafened: it hears nobody. Shown for the player of this client only; another pilot deafened is muted
    /// too, and shown so.
    Deafened,
    /// Muted by an administrator of the server: the game drops its voice. Red.
    MutedByAdmin,
};

/// The icons that are drawn, None excluded, in the order of VoiceIcon.
inline constexpr std::size_t VoiceIconCount = 6;

/// The texture of each icon, in the order of VoiceIcon from Speaking: white, tinted with voiceIconColor().
using VoiceIconTextures = std::array<render::TextureId, VoiceIconCount>;

/// The voice of the player of this client, for its line of the HUD.
struct LocalVoice {
    /// Its voice goes out now (voice::GameVoice::transmitting()).
    bool transmitting = false;
    /// Muted: by its key, or its voice turned off.
    bool muted = false;
    bool deafened = false;
    /// Muted by an administrator: the game drops its voice.
    bool mutedByAdmin = false;
    /// What carries its voice now: the game, or Discord once its call is connected. Told in the pause menu.
    voice::Backend backend = voice::Backend::None;

    friend bool operator==(const LocalVoice &, const LocalVoice &) = default;
};

/// The voice of a game, as the HUD shows it: the player of this client and the others, by slot.
struct VoiceView {
    LocalVoice local;
    std::vector<voice::Participant> others;
};

/// The icon of another pilot. Muted by an administrator first, as nothing else can change it; then muted by
/// me, as it is my choice; then talking; then what it told.
[[nodiscard]] VoiceIcon voiceIconOf(const voice::Participant &pilot);
/// The icon of the player of this client: muted by an administrator first, then deafened, then muted, then
/// talking.
[[nodiscard]] VoiceIcon voiceIconOf(const LocalVoice &local);

/// The colour of an icon (Série 16): white while talking, grey for a choice, dark grey without a microphone,
/// red when an administrator muted it.
[[nodiscard]] render::Color voiceIconColor(VoiceIcon icon);

/// The texture of an icon among those given; none for VoiceIcon::None.
[[nodiscard]] const render::TextureId *voiceIconTexture(const VoiceIconTextures &textures, VoiceIcon icon);

} // namespace rtype::client
