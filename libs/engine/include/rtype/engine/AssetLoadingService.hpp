#pragma once

#include "rtype/core/BackgroundLoader.hpp"

#include <memory>
#include <mutex>

namespace rtype::engine
{

class AssetLoadingService
{
    std::mutex _mutex;
    std::shared_ptr<core::BackgroundLoader> _queue;

public:
    [[nodiscard]] std::shared_ptr<core::BackgroundLoader> queue()
    {
        std::lock_guard lock(_mutex);
        if (!_queue) {
            _queue = std::make_shared<core::BackgroundLoader>();
        }
        return _queue;
    }
};

} // namespace rtype::engine
