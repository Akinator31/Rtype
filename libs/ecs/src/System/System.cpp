#include "rtype/ecs/System/System.hpp"

namespace rtype::ecs
{

System::~System() = default;

const std::vector<std::type_index> &System::componentTypes() const
{
    return _componentTypes;
}

} // namespace rtype::ecs
