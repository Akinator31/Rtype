#pragma once

#include "rtype/core/BackgroundLoader.hpp"

#include <memory>
#include <thread>

namespace rtype::engine
{

class ThreadManager
{
    std::thread::id _main = std::this_thread::get_id();
    std::shared_ptr<core::BackgroundLoader> _assets = std::make_shared<core::BackgroundLoader>();

public:
    ThreadManager() = default;
    ThreadManager(const ThreadManager &) = delete;
    ThreadManager &operator=(const ThreadManager &) = delete;
    ~ThreadManager();

    [[nodiscard]] std::shared_ptr<core::BackgroundLoader> getAssetsWorker() const { return _assets; }
    [[nodiscard]] bool isMainThread() const { return std::this_thread::get_id() == _main; }
    void stopAll();
};

} // namespace rtype::engine
