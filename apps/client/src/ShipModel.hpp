#pragma once

#include "rtype/render/ResourceManager.hpp"

#include <string>

namespace rtype::client
{

/// The animations of a ship model, by index (render::ResourceManager::getModelAnimation()): small flame when
/// it hovers or rests, big flame when it flies, nose up or down when it climbs or goes down, and the recoil
/// of its gun with the flash at its muzzle when it fires, added over the others.
struct ShipAnimations {
    int idle = 0;
    int move = 0;
    int moveUp = 0;
    int moveDown = 0;
    int shoot = 0;
    /// Length of the Shoot animation, in seconds.
    float shootDuration = 0.0f;
};

/// A ship model with its animations: the menu and the game fly the same models, made with the same rig.
struct ShipModel {
    render::ModelId model;
    ShipAnimations animations;
};

/// Loads a ship model and finds its animations.
/// @param path Path of the model, relative to the assets directory.
/// @throws std::runtime_error if the model cannot be loaded, std::out_of_range if it misses an animation.
[[nodiscard]] ShipModel loadShipModel(render::ResourceManager &resources, const std::string &path);

} // namespace rtype::client
