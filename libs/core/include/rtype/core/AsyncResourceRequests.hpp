#pragma once

#include "rtype/core/BackgroundLoader.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtype::core
{

template <typename Id>
class AsyncResourceRequests
{
    BackgroundLoader _loader;
    std::mutex _mutex;
    std::unordered_map<std::string, std::shared_future<Id>> _requests;
    std::vector<std::function<bool()>> _pending;

public:
    AsyncResourceRequests() = default;
    AsyncResourceRequests(const AsyncResourceRequests &) = delete;
    AsyncResourceRequests &operator=(const AsyncResourceRequests &) = delete;

    template <typename Prepare, typename Finish>
    std::shared_future<Id> request(std::string key, Prepare prepare, Finish finish)
    {
        std::lock_guard lock(_mutex);
        if (const auto found = _requests.find(key); found != _requests.end()) {
            return found->second;
        }
        auto prepared =
            std::make_shared<std::future<std::invoke_result_t<Prepare>>>(_loader.submit(std::move(prepare)));
        auto result = std::make_shared<std::promise<Id>>();
        std::shared_future<Id> shared = result->get_future().share();
        _pending.emplace_back([this, key, prepared, result, finish = std::move(finish)]() mutable {
            if (prepared->wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return false;
            }
            try {
                result->set_value(finish(prepared->get()));
            } catch (...) {
                result->set_exception(std::current_exception());
                _requests.erase(key);
            }
            return true;
        });
        _requests.emplace(std::move(key), shared);
        return shared;
    }

    void pump()
    {
        std::lock_guard lock(_mutex);
        for (auto pending = _pending.begin(); pending != _pending.end(); ++pending) {
            if ((*pending)()) {
                _pending.erase(pending);
                break;
            }
        }
    }
};

} // namespace rtype::core
