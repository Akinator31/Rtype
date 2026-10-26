#include "rtype/engine/ThreadManager.hpp"

namespace rtype::engine
{

ThreadManager::~ThreadManager()
{
    stopAll();
}

void ThreadManager::stopAll()
{
    _assets->stop();
}

} // namespace rtype::engine
