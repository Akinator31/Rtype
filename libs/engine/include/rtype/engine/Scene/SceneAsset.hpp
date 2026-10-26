#pragma once

#include <compare>
#include <functional>
#include <string>

namespace rtype::engine
{

enum class AssetType { Texture, SmoothTexture, Font, Model, Sound, Music };

struct SceneAsset {
    AssetType type;
    std::string path;

    auto operator<=>(const SceneAsset &) const = default;
};

struct SceneAssetRequest {
    std::function<bool()> ready;
    std::function<void()> complete;
    std::function<void()> release;
};

class SceneAssetLoader
{
public:
    virtual ~SceneAssetLoader() = default;
    virtual SceneAssetRequest request(const SceneAsset &asset) = 0;
};

enum class SceneLoadState { Idle, Loading, Failed };

}
