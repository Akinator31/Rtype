#include "LightingShader.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Lighting.hpp"

#include <cmath>
#include <vector>

namespace rtype::render::detail
{

namespace
{

::Vector3 toVector(Color color)
{
    return {static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
            static_cast<float>(color.b) / 255.0f};
}

::Vector3 normalized(Vector3 vector)
{
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    if (length == 0.0f) {
        return {0.0f, -1.0f, 0.0f};
    }
    return {vector.x / length, vector.y / length, vector.z / length};
}

// Tells whether a mesh of the model has a material with a normal map
bool hasNormalMap(const Model &model)
{
    for (int mesh = 0; mesh < model.meshCount; ++mesh) {
        if (model.materials[model.meshMaterial[mesh]].maps[MATERIAL_MAP_NORMAL].texture.id > 0) {
            return true;
        }
    }
    return false;
}

} // namespace

LightingShader::LightingShader()
    : _shader(LoadShaderFromMemory(shaders::lightingVertex, shaders::lightingFragment))
{
    _lightDirectionLocation = GetShaderLocation(_shader, "lightDirection");
    _lightColorLocation = GetShaderLocation(_shader, "lightColor");
    _ambientColorLocation = GetShaderLocation(_shader, "ambientColor");
    _useNormalMapLocation = GetShaderLocation(_shader, "useNormalMap");
    _available = IsShaderValid(_shader) && _lightDirectionLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the lighting shader: 3D models are drawn unlit");
    }
}

LightingShader::~LightingShader()
{
    UnloadShader(_shader);
}

void LightingShader::setLight(const Light &light, bool useNormalMap)
{
    const ::Vector3 direction = normalized(light.direction);
    const ::Vector3 color = toVector(light.color);
    const ::Vector3 ambient = toVector(light.ambient);
    const int normalMap = useNormalMap ? 1 : 0;
    SetShaderValue(_shader, _lightDirectionLocation, &direction, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _lightColorLocation, &color, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _ambientColorLocation, &ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _useNormalMapLocation, &normalMap, SHADER_UNIFORM_INT);
}

std::vector<Material> LightingShader::createLitMaterials(const Model &model) const
{
    std::vector<Material> materials(model.materials, model.materials + model.materialCount);
    for (Material &material : materials) {
        material.shader = _shader;
    }
    return materials;
}

void LightingShader::draw(const Model &model, const Light &light, ::Vector3 position, ::Vector3 rotationAxis,
                          float rotationAngle, float scale, ::Color tint)
{
    const ::Vector3 scales{scale, scale, scale};
    if (!_available) {
        DrawModelEx(model, position, rotationAxis, rotationAngle, scales, tint);
        return;
    }
    setLight(light, hasNormalMap(model));
    std::vector<Material> materials = createLitMaterials(model);
    Model lit = model;
    lit.materials = materials.data();
    DrawModelEx(lit, position, rotationAxis, rotationAngle, scales, tint);
}

} // namespace rtype::render::detail
