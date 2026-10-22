#include "GlowPass.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Blur.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::render::detail
{

namespace
{

// The glow is drawn at a quarter of the resolution of the frame: once blurred, the difference does not show,
// and the blur reaches four times further for the same cost
constexpr float ResolutionDivider = 4.0f;
// Number of horizontal then vertical blurs: each one spreads the light further
constexpr int BlurPasses = 2;
// How much brighter the glow is added over the frame than the models it comes from: the blur spreads their
// light over a larger area, so it would be faint otherwise
constexpr float GlowGain = 1.25f;

// Note: raylib colour macros cannot be used in this namespace, where Color is the type of the render library
constexpr ::Color Black{0, 0, 0, 255};
constexpr ::Color White{255, 255, 255, 255};

bool isUsable(const RenderTexture2D &image, int width, int height)
{
    return IsRenderTextureValid(image) && image.texture.width == width && image.texture.height == height;
}

} // namespace

GlowPass::GlowPass() : _shader(LoadShaderFromMemory(nullptr, shaders::blurFragment))
{
    _directionLocation = GetShaderLocation(_shader, "direction");
    _gainLocation = GetShaderLocation(_shader, "gain");
    _available = IsShaderValid(_shader) && _directionLocation >= 0 && _gainLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the blur shader: energy models do not glow");
    }
}

GlowPass::~GlowPass()
{
    for (const RenderTexture2D &image : _images) {
        if (IsRenderTextureValid(image)) {
            UnloadRenderTexture(image);
        }
    }
    UnloadShader(_shader);
}

void GlowPass::fit(int width, int height)
{
    for (RenderTexture2D &image : _images) {
        if (isUsable(image, width, height)) {
            continue;
        }
        if (IsRenderTextureValid(image)) {
            UnloadRenderTexture(image);
        }
        image = LoadRenderTexture(width, height);
        // Smooth when stretched over the frame, and no light wrapping around from the opposite edge
        SetTextureFilter(image.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(image.texture, TEXTURE_WRAP_CLAMP);
    }
}

void GlowPass::blur(const RenderTexture2D &from, const RenderTexture2D &to, ::Vector2 direction, float gain)
{
    const auto width = static_cast<float>(from.texture.width);
    const auto height = static_cast<float>(from.texture.height);
    SetShaderValue(_shader, _directionLocation, &direction, SHADER_UNIFORM_VEC2);
    SetShaderValue(_shader, _gainLocation, &gain, SHADER_UNIFORM_FLOAT);
    BeginTextureMode(to);
    ClearBackground(Black);
    BeginShaderMode(_shader);
    // Render textures are stored upside down: a negative height keeps the image the right way up
    DrawTexturePro(from.texture, {0.0f, 0.0f, width, -height}, {0.0f, 0.0f, width, height}, {0.0f, 0.0f},
                   0.0f, White);
    EndShaderMode();
    EndTextureMode();
}

const Texture2D *GlowPass::render(::Vector2 frameResolution, const std::function<void()> &drawModels)
{
    if (!_available) {
        return nullptr;
    }
    const int width = std::max(1, static_cast<int>(std::lround(frameResolution.x / ResolutionDivider)));
    const int height = std::max(1, static_cast<int>(std::lround(frameResolution.y / ResolutionDivider)));
    fit(width, height);
    if (!isUsable(_images[0], width, height) || !isUsable(_images[1], width, height)) {
        return nullptr;
    }

    BeginTextureMode(_images[0]);
    ClearBackground(Black);
    drawModels();
    EndTextureMode();

    const ::Vector2 texel{1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)};
    for (int pass = 0; pass < BlurPasses; ++pass) {
        const float gain = pass == BlurPasses - 1 ? GlowGain : 1.0f;
        blur(_images[0], _images[1], {texel.x, 0.0f}, 1.0f);
        blur(_images[1], _images[0], {0.0f, texel.y}, gain);
    }
    return &_images[0].texture;
}

} // namespace rtype::render::detail
