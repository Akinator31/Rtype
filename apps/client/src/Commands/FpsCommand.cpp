#include "Commands/FpsCommand.hpp"

#include "Commands/Switch.hpp"

namespace rtype::client
{

namespace
{

/// Room for "240 FPS" from the right edge of the screen, and space from its top, in game units.
constexpr float counterWidth = 130.0f;
constexpr float counterTop = 24.0f;

} // namespace

void registerFpsCommand(Console &console, bool &shown)
{
    console.registerConsoleCommand("fps", "fps [on|off]: shows or hides the number of frames per second",
                                   [&shown](Console &target, const Console::Arguments &arguments) {
                                       if (const std::optional<bool> wanted =
                                               readSwitch(target, arguments, shown, "fps [on|off]")) {
                                           shown = *wanted;
                                           target.print(shown ? "Frame rate shown" : "Frame rate hidden");
                                       }
                                   });
}

void drawFpsCounter(render::Renderer &renderer)
{
    const render::Rectangle area = renderer.visibleArea();
    renderer.drawFps({area.x + area.width - counterWidth, area.y + counterTop});
}

} // namespace rtype::client
