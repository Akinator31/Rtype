#pragma once

#include "Console/Console.hpp"
#include "Game/RemoteSession.hpp"

namespace rtype::client
{

/// "voice [on|off]": turns the microphone on or off in the games online, for an open microphone: the others
/// hear the player while it is on. Without an argument, it switches it. Off at each start; the talk modes and
/// their settings come later.
/// @param voice The devices of the voice, read by the sessions online. They must outlive the console.
void registerVoiceCommand(Console &console, VoiceDevices &voice);

} // namespace rtype::client
