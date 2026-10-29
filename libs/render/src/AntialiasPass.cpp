#include "AntialiasPass.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Antialiasing.hpp"

namespace rtype::render::detail
{

namespace
{

// Note: raylib colour macros cannot be used in this namespace, where Color is the type of the render library
constexpr ::Color Black{0, 0, 0, 255};
constexpr ::Color White{255, 255, 255, 255};

// Draws a whole image over another one of the same size. Render textures are stored upside down: a negative
// height keeps the image the right way up.
void copy(const RenderTexture2D &from)
{
    const auto width = static_cast<float>(from.texture.width);
    const auto height = static_cast<float>(from.texture.height);
    DrawTexturePro(from.texture, {0.0f, 0.0f, width, -height}, {0.0f, 0.0f, width, height}, {0.0f, 0.0f},
                   0.0f, White);
}

} // namespace

AntialiasPass::AntialiasPass() : _shader(LoadShaderFromMemory(nullptr, shaders::antialiasingFragment))
{
    _texelLocation = GetShaderLocation(_shader, "texel");
    _available = IsShaderValid(_shader) && _texelLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the antialiasing shader: the edges of the 3D models "
                                      "are not smoothed");
    }
}

AntialiasPass::~AntialiasPass()
{
    if (IsRenderTextureValid(_image)) {
        UnloadRenderTexture(_image);
    }
    UnloadShader(_shader);
}

bool AntialiasPass::fit(int width, int height)
{
    if (IsRenderTextureValid(_image) && _image.texture.width == width && _image.texture.height == height) {
        return true;
    }
    if (IsRenderTextureValid(_image)) {
        UnloadRenderTexture(_image);
    }
    _image = LoadRenderTexture(width, height);
    return IsRenderTextureValid(_image);
}

void AntialiasPass::apply(const RenderTexture2D &frame)
{
    if (!_available || !fit(frame.texture.width, frame.texture.height)) {
        return;
    }
    const ::Vector2 texel{1.0f / static_cast<float>(frame.texture.width),
                          1.0f / static_cast<float>(frame.texture.height)};
    SetShaderValue(_shader, _texelLocation, &texel, SHADER_UNIFORM_VEC2);

    BeginTextureMode(_image);
    ClearBackground(Black);
    BeginShaderMode(_shader);
    copy(frame);
    EndShaderMode();
    EndTextureMode();

    // The smoothed pixels are opaque: they replace those of the frame
    BeginTextureMode(frame);
    copy(_image);
    EndTextureMode();
}

} // namespace rtype::render::detail
