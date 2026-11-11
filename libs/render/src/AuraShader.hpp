#pragma once

// Private to the render library: the shader of the auras around the 3D models.

#include "rtype/render/Aura.hpp"

#include <raylib.h>

#include <vector>

namespace rtype::render::detail
{

/// Owns the aura shader (libs/shaders/glsl/Aura.vert and Aura.frag): a shell around a model, its vertices
/// pushed out along their normals, lit along the silhouette in the colour of the aura and added to what is
/// behind it, without hiding anything. If the shader cannot be compiled, a warning is logged and no aura is
/// drawn.
class AuraShader
{
private:
    Shader _shader{};
    int _colorLocation = -1;
    int _thicknessLocation = -1;
    int _viewPositionLocation = -1;
    int _viewDirectionLocation = -1;
    int _orthographicLocation = -1;
    int _timeLocation = -1;
    /// Whether the mesh drawn moves with the bones of its model (GPU skinning)
    int _skinnedLocation = -1;
    bool _available = false;

public:
    AuraShader();
    ~AuraShader();

    AuraShader(const AuraShader &) = delete;
    AuraShader &operator=(const AuraShader &) = delete;
    AuraShader(AuraShader &&) = delete;
    AuraShader &operator=(AuraShader &&) = delete;

    /// Draws the aura of a model placed like DrawModelEx(), turned first by spinAngle degrees around its own
    /// vertical axis, without its hidden meshes (by index). Must be called in 3D mode, through `camera`.
    /// @param strength Multiplies the brightness of the aura: its bloom, in the glow of the frame.
    void draw(const Model &model, const Aura &aura, const Camera3D &camera, ::Vector3 position,
              ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale,
              const std::vector<int> &hiddenMeshes, float strength = 1.0f);
};

} // namespace rtype::render::detail
