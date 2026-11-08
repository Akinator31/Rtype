#include "Commands/VoiceCommand.hpp"

#include "Commands/Switch.hpp"

namespace rtype::client
{

void registerVoiceCommand(Console &console, VoiceDevices &voice)
{
    console.registerConsoleCommand(
        "voice", "voice [on|off]: turns the microphone on or off in the games online",
        [&voice](Console &target, const Console::Arguments &arguments) {
            if (const std::optional<bool> wanted =
                    readSwitch(target, arguments, voice.microphoneOn, "voice [on|off]")) {
                voice.microphoneOn = *wanted;
                target.print(voice.microphoneOn ? "Microphone on: the others hear you" : "Microphone off");
            }
        });
}

} // namespace rtype::client
