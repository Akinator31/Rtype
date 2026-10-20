#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace rtype::core
{

/// Loads each resource once and keeps it until the cache is destroyed.
///
/// Resources are identified by a key, usually their file path: loading a key again returns the handle of
/// the resource already loaded. Handles stay valid for as long as the cache exists. The cache does not know
/// how to load a resource, the caller provides the loader, so it works with any kind of resource.
/// @tparam Resource Type of the stored resources. It frees what it holds in its destructor.
/// @tparam Id Handle type returned for this kind of resource.
template <typename Resource, typename Id>
class ResourceCache
{
private:
    mutable std::mutex _mutex;
    std::deque<Resource> _resources;
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

    /// Returns the resource with this handle. The reference stays valid until the cache is destroyed.
    /// @throws std::out_of_range if the handle does not come from this cache.
    [[nodiscard]] const Resource &get(Id id) const
    {
        std::lock_guard lock(_mutex);
        return _resources.at(id.index);
    }
    /// Returns the resource with this handle, to modify it.
    /// @throws std::out_of_range if the handle does not come from this cache.
    [[nodiscard]] Resource &get(Id id)
    {
        std::lock_guard lock(_mutex);
        return _resources.at(id.index);
    }

    /// Number of resources loaded.
    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lock(_mutex);
        return _resources.size();
    }
};

} // namespace rtype::core
