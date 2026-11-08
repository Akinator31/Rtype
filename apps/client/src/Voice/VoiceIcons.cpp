#include "Voice/VoiceIcons.hpp"

namespace rtype::client
{

namespace
{

// The colours of the Figma design, Série 16
constexpr render::Color Lit{255, 255, 255, 255};
constexpr render::Color Grey{138, 144, 151, 255};
constexpr render::Color DarkGrey{107, 113, 119, 255};

} // namespace

VoiceIcon voiceIconOf(const voice::Participant &pilot)
{
    if (pilot.mutedByMe) {
        return VoiceIcon::MutedByMe;
    }
    if (pilot.speaking) {
        return VoiceIcon::Speaking;
    }
    if (pilot.status.muted || pilot.status.deafened) {
        return VoiceIcon::Muted;
    }
    if (pilot.status.noMicrophone) {
        return VoiceIcon::NoMicrophone;
    }
    return VoiceIcon::None;
}

VoiceIcon voiceIconOf(const LocalVoice &local)
{
    if (local.deafened) {
        return VoiceIcon::Deafened;
    }
    if (local.muted) {
        return VoiceIcon::Muted;
    }
    return local.transmitting ? VoiceIcon::Speaking : VoiceIcon::None;
}

render::Color voiceIconColor(VoiceIcon icon)
{
    switch (icon) {
    case VoiceIcon::Speaking:
        return Lit;
    case VoiceIcon::NoMicrophone:
        return DarkGrey;
    default:
        return Grey;
    }
}

const render::TextureId *voiceIconTexture(const VoiceIconTextures &textures, VoiceIcon icon)
{
    if (icon == VoiceIcon::None) {
        return nullptr;
    }
    return &textures[static_cast<std::size_t>(icon) - 1];
}

} // namespace rtype::client
