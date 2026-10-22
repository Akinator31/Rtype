#include "EnergyShader.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Energy.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <vector>

namespace rtype::render::detail
{

namespace
{

// The model turned around its own vertical axis: raylib applies the transform of the model before the
// rotation given to DrawModelEx()
Model spin(const Model &model, float angle)
{
    Model spun = model;
    spun.transform = MatrixMultiply(MatrixRotateY(angle * DEG2RAD), model.transform);
    return spun;
}

} // namespace

EnergyShader::EnergyShader() : _shader(LoadShaderFromMemory(shaders::energyVertex, shaders::energyFragment))
{
    _viewPositionLocation = GetShaderLocation(_shader, "viewPosition");
    _timeLocation = GetShaderLocation(_shader, "time");
    _boostLocation = GetShaderLocation(_shader, "boost");
    _dissolveLocation = GetShaderLocation(_shader, "dissolve");
    _available = IsShaderValid(_shader) && _timeLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the energy shader: shots are drawn without it");
        return;
    }
    _shader.locs[SHADER_LOC_MAP_HEIGHT] = GetShaderLocation(_shader, "noiseMap");
}

EnergyShader::~EnergyShader()
{
    UnloadShader(_shader);
}

void EnergyShader::setUniforms(const Energy &energy, ::Vector3 viewPosition)
{
    const auto time = static_cast<float>(GetTime());
    SetShaderValue(_shader, _viewPositionLocation, &viewPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _timeLocation, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _boostLocation, &energy.boost, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _dissolveLocation, &energy.dissolve, SHADER_UNIFORM_FLOAT);
}

void EnergyShader::draw(const Model &model, const Energy &energy, const Texture2D &noise,
                        ::Vector3 viewPosition, ::Vector3 position, ::Vector3 rotationAxis,
                        float rotationAngle, float spinAngle, float scale, ::Color tint)
{
    const ::Vector3 scales{scale, scale, scale};
    Model drawn = spin(model, spinAngle);
    if (!_available) {
        DrawModelEx(drawn, position, rotationAxis, rotationAngle, scales, tint);
        return;
    }
    setUniforms(energy, viewPosition);
    std::vector<Material> materials(model.materials, model.materials + model.materialCount);
    for (Material &material : materials) {
        material.shader = _shader;
        material.maps[MATERIAL_MAP_HEIGHT].texture = noise;
    }
    drawn.materials = materials.data();

    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    DrawModelEx(drawn, position, rotationAxis, rotationAngle, scales, tint);
    rlEnableDepthMask();
    EndBlendMode();
}

} // namespace rtype::render::detail
