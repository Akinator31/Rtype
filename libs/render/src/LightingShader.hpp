#pragma once

// Private to the render library: the shader lighting the 3D models with a sun, a rim light and an ambient
// light.

#include "rtype/render/Types.hpp"

#include <raylib.h>

#include <vector>

namespace rtype::render::detail
{

/// Owns the lighting shader of the 3D models (libs/shaders/glsl/Lighting.frag): physically based, with the
/// metalness, roughness, occlusion, normal and emission maps of their materials. Materials without some of
/// these maps get neutral textures in their place. If the shader cannot be compiled, a warning is logged and
/// models are drawn unlit, with the colours of their textures.
class LightingShader
{
private:
    Shader _shader{};
    Texture2D _white{};
    Texture2D _black{};
    int _lightDirectionLocation = -1;
    int _lightColorLocation = -1;
    int _ambientColorLocation = -1;
    int _rimDirectionLocation = -1;
    int _rimColorLocation = -1;
    int _upDirectionLocation = -1;
    int _viewPositionLocation = -1;
    int _viewDirectionLocation = -1;
    int _orthographicLocation = -1;
    int _useNormalMapLocation = -1;
    int _roughnessLocation = -1;
    int _metalnessLocation = -1;
    int _emissionOnlyLocation = -1;
    int _emissionColorLocation = -1;
    bool _available = false;

    /// Sends the camera, the lights and whether only the emitted light is drawn to the shader.
    void setScene(const Light &light, const Camera3D &camera, bool emissionOnly);
    /// Turns a copy of a material, and of its maps, into one drawn with the lighting shader: its missing maps
    /// replaced, its factors sent to the shader.
    void prepareMaterial(Material &material);
    /// Draws the shown meshes of a model, like DrawModelEx(), turned first by spinAngle degrees around its
    /// own vertical axis, with the lighting shader if `lit`.
    void drawMeshes(const Model &model, ::Vector3 position, ::Vector3 rotationAxis, float rotationAngle,
                    float spinAngle, float scale, ::Color tint, const std::vector<int> &hiddenMeshes,
                    bool lit);

public:
    LightingShader();
    ~LightingShader();

    LightingShader(const LightingShader &) = delete;
    LightingShader &operator=(const LightingShader &) = delete;
    LightingShader(LightingShader &&) = delete;
    LightingShader &operator=(LightingShader &&) = delete;

    /// Tells whether a material of the model emits light, with an emission map or an emissive colour, so
    /// that it glows (drawEmission()).
    [[nodiscard]] static bool hasEmission(const Model &model);

    /// Draws a model lit by the light, like DrawModelEx(), turned first by spinAngle degrees around its own
    /// vertical axis, without its hidden meshes (by index). Must be called in 3D mode, through `camera`.
    void draw(const Model &model, const Light &light, const Camera3D &camera, ::Vector3 position,
              ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale, ::Color tint,
              const std::vector<int> &hiddenMeshes);
    /// Draws only the light the model emits, black elsewhere, so that it hides the glow behind it: the glow
    /// of the frame (Renderer::drawGlow()). Does nothing if the shader is not available. The parameters are
    /// those of draw().
    void drawEmission(const Model &model, const Camera3D &camera, ::Vector3 position, ::Vector3 rotationAxis,
                      float rotationAngle, float spinAngle, float scale, ::Color tint,
                      const std::vector<int> &hiddenMeshes);
};

} // namespace rtype::render::detail
