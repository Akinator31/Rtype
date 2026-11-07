#include "Instance/RecentDestructions.hpp"

namespace rtype::server
{

void RecentDestructions::add(std::uint32_t tick,
                             const std::vector<rtype::game::EntitiesDestroyedMessage> &messages)
{
    _messages.insert(_messages.end(), messages.begin(), messages.end());
    while (!_messages.empty() && _messages.front().tick + Repeat < tick) {
        _messages.pop_front();
    }
}

std::vector<rtype::game::EntitiesDestroyedMessage> RecentDestructions::again(std::uint32_t tick) const
{
    std::vector<rtype::game::EntitiesDestroyedMessage> messages;
    for (const rtype::game::EntitiesDestroyedMessage &message : _messages) {
        if (message.tick < tick && message.tick + Repeat >= tick) {
            messages.push_back(message);
        }
    }
    return messages;
}

} // namespace rtype::server
