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
// The wide halo is blurred again at half of the resolution of the glow, so that it reaches twice as far
constexpr int WideDivider = 2;
// How much of the wide halo is added to the glow: the light fades out slowly far from what shines, as in a
// lens, instead of stopping at the edge of the blur
constexpr float WideGain = 0.7f;

// Note: raylib colour macros cannot be used in this namespace, where Color is the type of the render library
constexpr ::Color Black{0, 0, 0, 255};
constexpr ::Color White{255, 255, 255, 255};

bool isUsable(const RenderTexture2D &image, int width, int height)
{
    return IsRenderTextureValid(image) && image.texture.width == width && image.texture.height == height;
}

// Draws a whole image over another one of any size. Render textures are stored upside down: a negative height
// keeps the image the right way up.
void stretch(const RenderTexture2D &from, const RenderTexture2D &to, ::Color tint)
{
    const auto width = static_cast<float>(from.texture.width);
    const auto height = static_cast<float>(from.texture.height);
    DrawTexturePro(from.texture, {0.0f, 0.0f, width, -height},
                   {0.0f, 0.0f, static_cast<float>(to.texture.width), static_cast<float>(to.texture.height)},
                   {0.0f, 0.0f}, 0.0f, tint);
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
    for (const Images *images : {&_images, &_wideImages}) {
        for (const RenderTexture2D &image : *images) {
            if (IsRenderTextureValid(image)) {
                UnloadRenderTexture(image);
            }
        }
    }
    UnloadShader(_shader);
}

bool GlowPass::fit(Images &images, int width, int height)
{
    for (RenderTexture2D &image : images) {
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
    return isUsable(images[0], width, height) && isUsable(images[1], width, height);
}

void GlowPass::blur(const RenderTexture2D &from, const RenderTexture2D &to, ::Vector2 direction, float gain)
{
    SetShaderValue(_shader, _directionLocation, &direction, SHADER_UNIFORM_VEC2);
    SetShaderValue(_shader, _gainLocation, &gain, SHADER_UNIFORM_FLOAT);
    BeginTextureMode(to);
    ClearBackground(Black);
    BeginShaderMode(_shader);
    stretch(from, to, White);
    EndShaderMode();
    EndTextureMode();
}

void GlowPass::blurBackAndForth(const Images &images, float gain)
{
    const ::Vector2 texel{1.0f / static_cast<float>(images[0].texture.width),
                          1.0f / static_cast<float>(images[0].texture.height)};
    for (int pass = 0; pass < BlurPasses; ++pass) {
        blur(images[0], images[1], {texel.x, 0.0f}, 1.0f);
        blur(images[1], images[0], {0.0f, texel.y}, pass == BlurPasses - 1 ? gain : 1.0f);
    }
}

const Texture2D *GlowPass::render(::Vector2 frameResolution, const std::function<void()> &drawModels)
{
    if (!_available) {
        return nullptr;
    }
    const int width = std::max(1, static_cast<int>(std::lround(frameResolution.x / ResolutionDivider)));
    const int height = std::max(1, static_cast<int>(std::lround(frameResolution.y / ResolutionDivider)));
    if (!fit(_images, width, height)) {
        return nullptr;
    }

    BeginTextureMode(_images[0]);
    ClearBackground(Black);
    drawModels();
    EndTextureMode();
    blurBackAndForth(_images, GlowGain);

    // The wide halo: the glow shrunk, blurred again, then stretched back over it. Without its images, the
    // glow is only narrower.
    if (!fit(_wideImages, std::max(1, width / WideDivider), std::max(1, height / WideDivider))) {
        return &_images[0].texture;
    }
    BeginTextureMode(_wideImages[0]);
    ClearBackground(Black);
    stretch(_images[0], _wideImages[0], White);
    EndTextureMode();
    blurBackAndForth(_wideImages, 1.0f);

    BeginTextureMode(_images[0]);
    BeginBlendMode(BLEND_ADDITIVE);
    const auto wideAlpha = static_cast<unsigned char>(std::lround(WideGain * 255.0f));
    stretch(_wideImages[0], _images[0], {255, 255, 255, wideAlpha});
    EndBlendMode();
    EndTextureMode();
    return &_images[0].texture;
}

} // namespace rtype::render::detail
