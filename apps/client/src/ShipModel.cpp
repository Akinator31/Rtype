#include "ShipModel.hpp"

namespace rtype::client
{

ShipModel loadShipModel(render::ResourceManager &resources, const std::string &path)
{
    const render::ModelId model = resources.loadModel(path);
    return {model,
            {resources.getModelAnimation(model, "Idle"), resources.getModelAnimation(model, "Move"),
             resources.getModelAnimation(model, "MoveUp"), resources.getModelAnimation(model, "MoveDown")}};
}

} // namespace rtype::client
