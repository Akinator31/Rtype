#include "ecs/System/System.hpp"

namespace ecs
{

System::~System() = default;

const std::vector<std::type_index> &System::componentTypes() const
{
    return _componentTypes;
}

} // namespace ecs
