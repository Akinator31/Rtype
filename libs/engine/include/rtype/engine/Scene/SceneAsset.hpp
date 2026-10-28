#pragma once

#include <compare>
#include <string>

namespace rtype::engine
{

/// The kinds of assets that a scene can need.
enum class AssetType { Texture, SmoothTexture, Font, Model, Sound, Music };

/// An asset that a scene needs (see SceneSettings::assets): the engine has it loaded before the scene opens,
/// and unloaded once no open scene needs it.
struct SceneAsset {
    AssetType type;
    /// Path of the file, relative to the assets directory, for instance "models/ship.glb".
    std::string path;

    auto operator<=>(const SceneAsset &) const = default;
};

} // namespace rtype::engine
