#include "rtype/engine/Quest/QuestSystem.hpp"

#include <utility>

namespace rtype::engine
{

QuestSystem::QuestSystem(std::string id, std::vector<std::string> events, Settings &settings)
    : _id(std::move(id)), _events(std::move(events)), _settings(settings)
{
}

const std::string &QuestSystem::getId() const
{
    return _id;
}

const std::vector<std::string> &QuestSystem::getEvents() const
{
    return _events;
}

std::string QuestSystem::getSettingsKey() const
{
    return "quest." + _id;
}

bool QuestSystem::isCompleted() const
{
    return _settings.getBool(getSettingsKey()).value_or(false);
}

void QuestSystem::setCompletionHandler(std::function<void(const QuestSystem &)> onCompleted)
{
    _onCompleted = std::move(onCompleted);
}

void QuestSystem::update(float /*elapsed*/)
{
    if (!isCompleted() && progress()) {
        complete();
    }
}

void QuestSystem::complete()
{
    _settings.setBool(getSettingsKey(), true);
    if (_onCompleted) {
        _onCompleted(*this);
    }
}

} // namespace rtype::engine
