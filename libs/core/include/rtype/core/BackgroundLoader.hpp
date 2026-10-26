#pragma once

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace rtype::core
{

class BackgroundLoaderCancelled : public std::runtime_error
{
public:
    BackgroundLoaderCancelled() : std::runtime_error("BackgroundLoader request cancelled") {}
};

class BackgroundLoader
{
    struct Task {
        std::function<void()> run;
        std::function<void()> cancel;
    };

    std::mutex _mutex;
    std::mutex _stopMutex;
    std::condition_variable _condition;
    std::deque<Task> _tasks;
    bool _stopping = false;
    std::thread _worker;

    template <typename Result, typename Function>
    static void complete(const std::shared_ptr<Function> &callable,
                         const std::shared_ptr<std::promise<Result>> &promise)
    {
        try {
            if constexpr (std::is_void_v<Result>) {
                std::invoke(*callable);
                promise->set_value();
            } else {
                promise->set_value(std::invoke(*callable));
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }

    template <typename Result, typename Function>
    static Task makeTask(std::shared_ptr<Function> callable,
                         std::shared_ptr<std::promise<Result>> promise)
    {
        return {
            [callable, promise] { complete<Result>(callable, promise); },
            [promise] {
                promise->set_exception(std::make_exception_ptr(BackgroundLoaderCancelled()));
            },
        };
    }

    void run()
    {
        while (true) {
            Task task;
            {
                std::unique_lock lock(_mutex);
                _condition.wait(lock, [this] { return _stopping || !_tasks.empty(); });
                if (_stopping) {
                    return;
                }
                task = std::move(_tasks.front());
                _tasks.pop_front();
            }
            task.run();
        }
    }

public:
    BackgroundLoader() : _worker([this] { run(); }) {}

    BackgroundLoader(const BackgroundLoader &) = delete;
    BackgroundLoader &operator=(const BackgroundLoader &) = delete;

    ~BackgroundLoader() { stop(); }

    void stop()
    {
        std::lock_guard stopLock(_stopMutex);
        std::deque<Task> pending;
        {
            std::lock_guard lock(_mutex);
            _stopping = true;
            pending.swap(_tasks);
        }
        for (Task &task : pending) {
            task.cancel();
        }
        _condition.notify_all();
        if (_worker.joinable()) {
            _worker.join();
        }
    }

    template <typename Function>
    auto submit(Function &&function) -> std::future<std::invoke_result_t<Function>>
    {
        using Result = std::invoke_result_t<Function>;
        auto callable = std::make_shared<std::decay_t<Function>>(std::forward<Function>(function));
        auto promise = std::make_shared<std::promise<Result>>();
        std::future<Result> result = promise->get_future();
        {
            std::lock_guard lock(_mutex);
            if (_stopping) {
                throw BackgroundLoaderCancelled();
            }
            _tasks.push_back(makeTask<Result>(std::move(callable), std::move(promise)));
        }
        _condition.notify_one();
        return result;
    }
};

} // namespace rtype::core
