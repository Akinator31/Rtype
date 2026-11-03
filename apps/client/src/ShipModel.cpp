#include "ShipModel.hpp"

namespace rtype::client
{

ShipModel loadShipModel(render::ResourceManager &resources, const std::string &path)
{
    const render::ModelId model = resources.loadModel(path);
    const int shoot = resources.getModelAnimation(model, "Shoot");
    return {model,
            {resources.getModelAnimation(model, "Idle"), resources.getModelAnimation(model, "Move"),
             resources.getModelAnimation(model, "MoveUp"), resources.getModelAnimation(model, "MoveDown"), shoot,
             resources.getModelAnimationDuration(model, shoot)}};
}

} // namespace rtype::client
