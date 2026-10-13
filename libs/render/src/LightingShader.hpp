#pragma once

// Private to the render library: the shader lighting the 3D models with a directional light.

#include "rtype/render/Types.hpp"

#include <raylib.h>

#include <vector>

namespace rtype::render::detail
{

/// Owns the lighting shader of the 3D models: a directional light, such as a sun, plus an ambient light,
/// with the normal map of the material when it has one. If the shader cannot be compiled, a warning is logged
/// and models are drawn unlit, with the colours of their textures.
class LightingShader
{
private:
    Shader _shader{};
    int _lightDirectionLocation = -1;
    int _lightColorLocation = -1;
    int _ambientColorLocation = -1;
    int _useNormalMapLocation = -1;
    bool _available = false;

    /// Sends the light to the shader, and whether the material of the model has a normal map.
    void setLight(const Light &light, bool useNormalMap);
    /// Copies of the materials of the model, drawn with the lighting shader: the model keeps its own.
    [[nodiscard]] std::vector<Material> createLitMaterials(const Model &model) const;

public:
    LightingShader();
    ~LightingShader();

    LightingShader(const LightingShader &) = delete;
    LightingShader &operator=(const LightingShader &) = delete;
    LightingShader(LightingShader &&) = delete;
    LightingShader &operator=(LightingShader &&) = delete;

    /// Draws a model lit by the light, like DrawModelEx(), turned first by spinAngle degrees around its own
    /// vertical axis. Must be called in 3D mode.
    void draw(const Model &model, const Light &light, ::Vector3 position, ::Vector3 rotationAxis,
              float rotationAngle, float spinAngle, float scale, ::Color tint);
};

} // namespace rtype::render::detail
