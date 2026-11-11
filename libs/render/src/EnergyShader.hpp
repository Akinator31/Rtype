#pragma once

// Private to the render library: the shader of the 3D models of energy.

#include "rtype/render/Energy.hpp"

#include <raylib.h>

namespace rtype::render::detail
{

/// Owns the energy shader (libs/shaders/glsl/Energy.frag): shots, beams and plasma orbs glow with the colour
/// of their texture, white hot where they face the camera, flickering with a noise that flows over them. They
/// are added to what is behind them and hide nothing. If the shader cannot be compiled, a warning is logged
/// and the models are drawn with the colours of their textures.
class EnergyShader
{
private:
    Shader _shader{};
    int _viewPositionLocation = -1;
    int _timeLocation = -1;
    int _boostLocation = -1;
    int _dissolveLocation = -1;
    /// Whether the mesh drawn moves with the bones of its model (GPU skinning)
    int _skinnedLocation = -1;
    bool _available = false;

    /// Sends the camera, the time and the look of the energy to the shader.
    void setUniforms(const Energy &energy, ::Vector3 viewPosition);

public:
    EnergyShader();
    ~EnergyShader();

    EnergyShader(const EnergyShader &) = delete;
    EnergyShader &operator=(const EnergyShader &) = delete;
    EnergyShader(EnergyShader &&) = delete;
    EnergyShader &operator=(EnergyShader &&) = delete;

    /// Draws a model of energy, like DrawModelEx(), turned first by spinAngle degrees around its own vertical
    /// axis. Must be called in 3D mode.
    /// @param noise The texture of energy.noise.
    /// @param viewPosition Where the camera is.
    void draw(const Model &model, const Energy &energy, const Texture2D &noise, ::Vector3 viewPosition,
              ::Vector3 position, ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale,
              ::Color tint);
};

} // namespace rtype::render::detail
