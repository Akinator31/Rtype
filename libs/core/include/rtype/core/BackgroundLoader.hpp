#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace rtype::core
{

class BackgroundLoader
{
    std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<std::function<void()>> _tasks;
    bool _stopping = false;
    std::thread _worker;

    void run()
    {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(_mutex);
                _condition.wait(lock, [this] { return _stopping || !_tasks.empty(); });
                if (_stopping && _tasks.empty()) {
                    return;
                }
                task = std::move(_tasks.front());
                _tasks.pop_front();
            }
            task();
        }
    }

public:
    BackgroundLoader() : _worker([this] { run(); }) {}

    BackgroundLoader(const BackgroundLoader &) = delete;
    BackgroundLoader &operator=(const BackgroundLoader &) = delete;

    ~BackgroundLoader()
    {
        {
            std::lock_guard lock(_mutex);
            _stopping = true;
        }
        _condition.notify_all();
        _worker.join();
    }

    template <typename Function>
    auto submit(Function &&function) -> std::future<std::invoke_result_t<Function>>
    {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
        std::future<Result> result = task->get_future();
        {
            std::lock_guard lock(_mutex);
            if (_stopping) {
                throw std::runtime_error("BackgroundLoader has stopped");
            }
            _tasks.emplace_back([task] { (*task)(); });
        }
        _condition.notify_one();
        return result;
    }
};

}
