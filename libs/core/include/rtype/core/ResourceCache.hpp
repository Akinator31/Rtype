#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace rtype::core
{

/// Loads each resource once, and keeps it until it is unloaded.
///
/// Resources are identified by a key, usually their file path: loading a key again returns the handle of
/// the resource already loaded. Handles stay valid until their resource is unloaded. The cache does not know
/// how to load a resource, the caller provides the loader, so it works with any kind of resource.
/// @tparam Resource Type of the stored resources. It frees what it holds in its destructor.
/// @tparam Id Handle type returned for this kind of resource.
template <typename Resource, typename Id>
class ResourceCache
{
private:
    mutable std::mutex _mutex;
    std::deque<std::optional<Resource>> _resources;
    std::unordered_map<std::string, Id> _ids;

public:
    /// Returns the handle of the resource with this key, loading it first if needed.
    /// @param key Identifies the resource, for instance its file path.
    /// @param loader Function taking no argument and returning the Resource. Only called if the key is not
    /// loaded yet.
    /// @throws Anything thrown by the loader. The cache is then unchanged and the key can be loaded again.
    template <typename Loader>
    Id load(const std::string &key, Loader &&loader)
    {
        std::lock_guard lock(_mutex);
        if (const auto it = _ids.find(key); it != _ids.end()) {
            return it->second;
        }
        _resources.push_back(std::forward<Loader>(loader)());
        const Id id{static_cast<std::uint32_t>(_resources.size() - 1)};
        try {
            _ids.emplace(key, id);
        } catch (...) {
            _resources.pop_back();
            throw;
        }
        return id;
    }

    /// Returns the resource with this handle. The reference stays valid until the resource is unloaded.
    /// @throws std::out_of_range if the handle does not come from this cache, or if its resource was
    /// unloaded.
    [[nodiscard]] const Resource &get(Id id) const
    {
        std::lock_guard lock(_mutex);
        const auto &resource = _resources.at(id.index);
        if (!resource) {
            throw std::out_of_range("Resource was unloaded");
        }
        return *resource;
    }
    /// Returns the resource with this handle, to modify it.
    /// @throws std::out_of_range if the handle does not come from this cache, or if its resource was
    /// unloaded.
    [[nodiscard]] Resource &get(Id id)
    {
        std::lock_guard lock(_mutex);
        auto &resource = _resources.at(id.index);
        if (!resource) {
            throw std::out_of_range("Resource was unloaded");
        }
        return *resource;
    }

    /// Number of resources loaded.
    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lock(_mutex);
        return _ids.size();
    }

    /// Returns the handle of the resource with this key if it is loaded, without loading it.
    /// @return Nothing if the key was never loaded, or was unloaded since.
    [[nodiscard]] std::optional<Id> findId(const std::string &key) const
    {
        std::lock_guard lock(_mutex);
        const auto found = _ids.find(key);
        return found == _ids.end() ? std::nullopt : std::optional<Id>(found->second);
    }

    /// Frees the resource with this key. Its handle becomes invalid: get() throws for it, and the cache never
    /// gives it again. Loading the key again loads the resource again, with a new handle.
    /// @return false if the key is not loaded: nothing changes.
    bool unload(const std::string &key)
    {
        std::lock_guard lock(_mutex);
        const auto found = _ids.find(key);
        if (found == _ids.end()) {
            return false;
        }
        _resources[found->second.index].reset();
        _ids.erase(found);
        return true;
    }
};

} // namespace rtype::core
