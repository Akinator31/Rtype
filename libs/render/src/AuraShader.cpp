#include "AuraShader.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Aura.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>

namespace rtype::render::detail
{

namespace
{

::Vector3 viewDirectionOf(const Camera3D &camera)
{
    const ::Vector3 direction = Vector3Subtract(camera.target, camera.position);
    const float length = Vector3Length(direction);
    return length > 0.0f ? Vector3Scale(direction, 1.0f / length) : ::Vector3{0.0f, 0.0f, -1.0f};
}

} // namespace

AuraShader::AuraShader() : _shader(LoadShaderFromMemory(shaders::auraVertex, shaders::auraFragment))
{
    _colorLocation = GetShaderLocation(_shader, "auraColor");
    _thicknessLocation = GetShaderLocation(_shader, "thickness");
    _viewPositionLocation = GetShaderLocation(_shader, "viewPosition");
    _viewDirectionLocation = GetShaderLocation(_shader, "viewDirection");
    _orthographicLocation = GetShaderLocation(_shader, "orthographic");
    _timeLocation = GetShaderLocation(_shader, "time");
    _available = IsShaderValid(_shader) && _colorLocation >= 0 && _thicknessLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the aura shader: the ships are drawn without auras");
    }
}

AuraShader::~AuraShader()
{
    UnloadShader(_shader);
}

void AuraShader::draw(const Model &model, const Aura &aura, const Camera3D &camera, ::Vector3 position,
                      ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale,
                      const std::vector<int> &hiddenMeshes, float strength)
{
    if (!_available) {
        return;
    }
    const std::array<float, 4> color{
        static_cast<float>(aura.color.r) / 255.0f, static_cast<float>(aura.color.g) / 255.0f,
        static_cast<float>(aura.color.b) / 255.0f,
        static_cast<float>(aura.color.a) / 255.0f * std::clamp(strength, 0.0f, 1.0f)};
    const ::Vector3 viewDirection = viewDirectionOf(camera);
    const int orthographic = camera.projection == CAMERA_ORTHOGRAPHIC ? 1 : 0;
    const auto time = static_cast<float>(GetTime());
    SetShaderValue(_shader, _colorLocation, color.data(), SHADER_UNIFORM_VEC4);
    SetShaderValue(_shader, _thicknessLocation, &aura.thickness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _viewPositionLocation, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _viewDirectionLocation, &viewDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _orthographicLocation, &orthographic, SHADER_UNIFORM_INT);
    SetShaderValue(_shader, _timeLocation, &time, SHADER_UNIFORM_FLOAT);

    // As DrawModelEx(): the transform of the model, turned around its own vertical axis first, then scaled,
    // rotated and moved
    const ::Matrix rotation = MatrixRotate(rotationAxis, rotationAngle * DEG2RAD);
    const ::Matrix placement = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), rotation),
                                              MatrixTranslate(position.x, position.y, position.z));
    const ::Matrix transform =
        MatrixMultiply(MatrixMultiply(MatrixRotateY(spinAngle * DEG2RAD), model.transform), placement);

    // Added over the model and what is behind it, hiding nothing; both sides of the shell, so that a mesh
    // open on one side still shows its edge
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    for (int mesh = 0; mesh < model.meshCount; ++mesh) {
        if (std::ranges::find(hiddenMeshes, mesh) != hiddenMeshes.end()) {
            continue;
        }
        Material material = model.materials[model.meshMaterial[mesh]];
        material.shader = _shader;
        DrawMesh(model.meshes[mesh], material, transform);
    }
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

} // namespace rtype::render::detail
