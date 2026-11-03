#include "LightingShader.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/Lighting.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtype::render::detail
{

namespace
{

// The roughness and metalness of materials without a metal/roughness map nor factors (a roughness of 0 is
// taken as none): a satin paint, which catches a wide highlight without looking wet
constexpr float DefaultRoughness = 0.65f;
constexpr float DefaultMetalness = 0.0f;

// Number of maps of a raylib material, MAX_MATERIAL_MAPS in rmodels.c, which raylib.h does not expose:
// DrawMesh() reads that many
constexpr std::size_t MaterialMapCount = 12;
static_assert(MaterialMapCount > MATERIAL_MAP_BRDF);

::Vector3 toVector(Color color)
{
    return {static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
            static_cast<float>(color.b) / 255.0f};
}

::Vector3 normalized(::Vector3 vector, ::Vector3 fallback)
{
    const float length = Vector3Length(vector);
    if (length == 0.0f) {
        return fallback;
    }
    return Vector3Scale(vector, 1.0f / length);
}

::Vector3 normalized(Vector3 vector)
{
    return normalized({vector.x, vector.y, vector.z}, {0.0f, -1.0f, 0.0f});
}

Texture2D createPlainTexture(std::uint8_t level)
{
    const Image image = GenImageColor(1, 1, ::Color{level, level, level, 255});
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

// Whether a material emits light: through an emission map, or all over with its emissive colour
bool emitsLight(const MaterialMap &emission)
{
    return emission.texture.id > 0 || emission.color.r > 0 || emission.color.g > 0 || emission.color.b > 0;
}

::Color multiply(::Color color, ::Color tint)
{
    const auto channel = [](std::uint8_t first, std::uint8_t second) {
        return static_cast<std::uint8_t>((static_cast<int>(first) * static_cast<int>(second)) / 255);
    };
    return {channel(color.r, tint.r), channel(color.g, tint.g), channel(color.b, tint.b),
            channel(color.a, tint.a)};
}

} // namespace

LightingShader::LightingShader()
    : _shader(LoadShaderFromMemory(shaders::lightingVertex, shaders::lightingFragment))
{
    _lightDirectionLocation = GetShaderLocation(_shader, "lightDirection");
    _lightColorLocation = GetShaderLocation(_shader, "lightColor");
    _ambientColorLocation = GetShaderLocation(_shader, "ambientColor");
    _rimDirectionLocation = GetShaderLocation(_shader, "rimDirection");
    _rimColorLocation = GetShaderLocation(_shader, "rimColor");
    _upDirectionLocation = GetShaderLocation(_shader, "upDirection");
    _viewPositionLocation = GetShaderLocation(_shader, "viewPosition");
    _viewDirectionLocation = GetShaderLocation(_shader, "viewDirection");
    _orthographicLocation = GetShaderLocation(_shader, "orthographic");
    _useNormalMapLocation = GetShaderLocation(_shader, "useNormalMap");
    _roughnessLocation = GetShaderLocation(_shader, "roughness");
    _metalnessLocation = GetShaderLocation(_shader, "metalness");
    _emissionOnlyLocation = GetShaderLocation(_shader, "emissionOnly");
    _emissionColorLocation = GetShaderLocation(_shader, "emissionColor");
    _available = IsShaderValid(_shader) && _lightDirectionLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the lighting shader: 3D models are drawn unlit");
        return;
    }
    // raylib binds texture0 to texture2 by itself: name the samplers of the other maps
    _shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(_shader, "roughnessMap");
    _shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(_shader, "occlusionMap");
    _shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(_shader, "emissionMap");
    _white = createPlainTexture(255);
    _black = createPlainTexture(0);
}

LightingShader::~LightingShader()
{
    for (const Texture2D &texture : {_white, _black}) {
        if (texture.id > 0) {
            UnloadTexture(texture);
        }
    }
    UnloadShader(_shader);
}

bool LightingShader::hasEmission(const Model &model)
{
    return std::any_of(model.materials, model.materials + model.materialCount, [](const Material &material) {
        return emitsLight(material.maps[MATERIAL_MAP_EMISSION]);
    });
}

void LightingShader::setScene(const Light &light, const Camera3D &camera, bool emissionOnly)
{
    const ::Vector3 direction = normalized(light.direction);
    const ::Vector3 color = toVector(light.color);
    const ::Vector3 ambient = toVector(light.ambient);
    const ::Vector3 rimDirection = normalized(light.rimDirection);
    const ::Vector3 rim = toVector(light.rim);
    const ::Vector3 up = normalized(camera.up, {0.0f, 1.0f, 0.0f});
    const ::Vector3 viewDirection =
        normalized(Vector3Subtract(camera.target, camera.position), {0.0f, 0.0f, -1.0f});
    const int orthographic = camera.projection == CAMERA_ORTHOGRAPHIC ? 1 : 0;
    const int emission = emissionOnly ? 1 : 0;
    SetShaderValue(_shader, _lightDirectionLocation, &direction, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _lightColorLocation, &color, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _ambientColorLocation, &ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _rimDirectionLocation, &rimDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _rimColorLocation, &rim, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _upDirectionLocation, &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _viewPositionLocation, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _viewDirectionLocation, &viewDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _orthographicLocation, &orthographic, SHADER_UNIFORM_INT);
    SetShaderValue(_shader, _emissionOnlyLocation, &emission, SHADER_UNIFORM_INT);
}

void LightingShader::prepareMaterial(Material &material)
{
    material.shader = _shader;
    MaterialMap *maps = material.maps;
    const bool hasMetalRoughness =
        maps[MATERIAL_MAP_ROUGHNESS].texture.id > 0 || maps[MATERIAL_MAP_ROUGHNESS].value > 0.0f;
    const float roughness = hasMetalRoughness ? maps[MATERIAL_MAP_ROUGHNESS].value : DefaultRoughness;
    const float metalness = hasMetalRoughness ? maps[MATERIAL_MAP_METALNESS].value : DefaultMetalness;
    const int useNormalMap = maps[MATERIAL_MAP_NORMAL].texture.id > 0 ? 1 : 0;
    // raylib leaves the texture of the previous mesh bound where a map has none
    for (const int map : {MATERIAL_MAP_METALNESS, MATERIAL_MAP_ROUGHNESS, MATERIAL_MAP_OCCLUSION}) {
        if (maps[map].texture.id == 0) {
            maps[map].texture = _white;
        }
    }
    // The emission map is multiplied by the emissive colour, white if it has none. Without a map, a material
    // emits its emissive colour all over, or nothing.
    MaterialMap &emission = maps[MATERIAL_MAP_EMISSION];
    const bool coloured = emission.color.r > 0 || emission.color.g > 0 || emission.color.b > 0;
    const Color factor{emission.color.r, emission.color.g, emission.color.b, 255};
    const ::Vector3 emissionColor = coloured ? toVector(factor) : ::Vector3{1.0f, 1.0f, 1.0f};
    if (emission.texture.id == 0) {
        emission.texture = coloured ? _white : _black;
    }
    SetShaderValue(_shader, _emissionColorLocation, &emissionColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(_shader, _roughnessLocation, &roughness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _metalnessLocation, &metalness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _useNormalMapLocation, &useNormalMap, SHADER_UNIFORM_INT);
}

void LightingShader::drawMeshes(const Model &model, ::Vector3 position, ::Vector3 rotationAxis,
                                float rotationAngle, float spinAngle, float scale, ::Color tint,
                                const std::vector<int> &hiddenMeshes, bool lit)
{
    // As DrawModelEx(): the transform of the model, turned around its own vertical axis first, then scaled,
    // rotated and moved
    const Matrix rotation = MatrixRotate(rotationAxis, rotationAngle * DEG2RAD);
    const Matrix placement = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), rotation),
                                            MatrixTranslate(position.x, position.y, position.z));
    const Matrix transform =
        MatrixMultiply(MatrixMultiply(MatrixRotateY(spinAngle * DEG2RAD), model.transform), placement);

    for (int mesh = 0; mesh < model.meshCount; ++mesh) {
        if (std::ranges::find(hiddenMeshes, mesh) != hiddenMeshes.end()) {
            continue;
        }
        // A copy of the material and of its maps: the model keeps its own
        Material material = model.materials[model.meshMaterial[mesh]];
        std::array<MaterialMap, MaterialMapCount> maps{};
        std::copy_n(material.maps, MaterialMapCount, maps.begin());
        material.maps = maps.data();
        if (lit) {
            prepareMaterial(material);
        }
        maps[MATERIAL_MAP_DIFFUSE].color = multiply(maps[MATERIAL_MAP_DIFFUSE].color, tint);
        const bool doubleSided = material.params[DoubleSidedParameter] > 0.0f;
        if (doubleSided) {
            rlDisableBackfaceCulling();
        }
        DrawMesh(model.meshes[mesh], material, transform);
        if (doubleSided) {
            rlEnableBackfaceCulling();
        }
    }
}

void LightingShader::draw(const Model &model, const Light &light, const Camera3D &camera, ::Vector3 position,
                          ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale,
                          ::Color tint, const std::vector<int> &hiddenMeshes)
{
    if (_available) {
        setScene(light, camera, false);
    }
    drawMeshes(model, position, rotationAxis, rotationAngle, spinAngle, scale, tint, hiddenMeshes,
               _available);
}

void LightingShader::drawEmission(const Model &model, const Camera3D &camera, ::Vector3 position,
                                  ::Vector3 rotationAxis, float rotationAngle, float spinAngle, float scale,
                                  ::Color tint, const std::vector<int> &hiddenMeshes)
{
    if (!_available) {
        return;
    }
    setScene(Light{}, camera, true);
    drawMeshes(model, position, rotationAxis, rotationAngle, spinAngle, scale, tint, hiddenMeshes, true);
}

} // namespace rtype::render::detail
