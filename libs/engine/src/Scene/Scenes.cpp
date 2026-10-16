#include "rtype/engine/Scene/Scenes.hpp"

#include <algorithm>
#include <utility>

namespace rtype::engine
{

Scenes::Scenes(ecs::ComponentCaches &caches) : _members(use<SceneMember>(caches)) {}

void Scenes::update(float /*elapsed*/)
{
    _inputOwner = top();
}

SceneId Scenes::open(SceneSettings settings)
{
    const SceneId id{_nextId++};
    _stack.push_back({id, std::move(settings)});
    return id;
}

void Scenes::close()
{
    if (!_stack.empty()) {
        close(_stack.back().id);
    }
}

void Scenes::close(SceneId scene)
{
    const auto entry = std::ranges::find(_stack, scene, &Entry::id);
    if (entry == _stack.end()) {
        return;
    }
    _stack.erase(entry);
    _closed.push_back(scene);
}

void Scenes::clear()
{
    while (!_stack.empty()) {
        close();
    }
}

void Scenes::back()
{
    if (_stack.empty()) {
        return;
    }
    // Copied: onBack may close the scene, and destroy the settings it is stored in
    const std::function<void()> onBack = _stack.back().settings.onBack;
    if (onBack) {
        onBack();
    } else {
        close();
    }
}

std::vector<SceneId> Scenes::takeClosedScenes()
{
    std::vector<SceneId> closed = std::move(_closed);
    _closed.clear();
    return closed;
}

const Scenes::Entry *Scenes::find(SceneId scene) const
{
    const auto entry = std::ranges::find(_stack, scene, &Entry::id);
    return entry == _stack.end() ? nullptr : &*entry;
}

std::optional<SceneId> Scenes::top() const
{
    return _stack.empty() ? std::nullopt : std::optional<SceneId>(_stack.back().id);
}

bool Scenes::isOpen(SceneId scene) const
{
    return find(scene) != nullptr;
}

bool Scenes::receivesInput(SceneId scene) const
{
    return _inputOwner == scene && isOpen(scene);
}

bool Scenes::updates(SceneId scene) const
{
    const Entry *entry = find(scene);
    return entry != nullptr && (top() == scene || entry->settings.updatesWhenCovered);
}

bool Scenes::isVisible(SceneId scene) const
{
    // Visible if every scene above it is an overlay
    for (auto entry = _stack.rbegin(); entry != _stack.rend(); ++entry) {
        if (entry->id == scene) {
            return true;
        }
        if (!entry->settings.overlay) {
            return false;
        }
    }
    return false;
}

bool Scenes::isInterfaceVisible(SceneId scene) const
{
    for (auto entry = _stack.rbegin(); entry != _stack.rend(); ++entry) {
        if (entry->id == scene) {
            return true;
        }
        if (!entry->settings.overlay || entry->settings.hidesInterfaceBelow) {
            return false;
        }
    }
    return false;
}

std::optional<SceneId> Scenes::sceneOf(ecs::entity_t entity) const
{
    return _members.has(entity) ? std::optional<SceneId>(_members.get(entity).scene) : std::nullopt;
}

bool Scenes::receivesInput(ecs::entity_t entity) const
{
    const std::optional<SceneId> scene = sceneOf(entity);
    return scene && receivesInput(*scene);
}

bool Scenes::updates(ecs::entity_t entity) const
{
    const std::optional<SceneId> scene = sceneOf(entity);
    return !scene || updates(*scene);
}

bool Scenes::isVisible(ecs::entity_t entity) const
{
    const std::optional<SceneId> scene = sceneOf(entity);
    return !scene || isVisible(*scene);
}

bool Scenes::isInterfaceVisible(ecs::entity_t entity) const
{
    const std::optional<SceneId> scene = sceneOf(entity);
    return !scene || isInterfaceVisible(*scene);
}

} // namespace rtype::engine
