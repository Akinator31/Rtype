#pragma once

#include <future>
#include <utility>

namespace rtype::core
{

/// Returns a future that already holds this value: what an asynchronous load gives for a resource that is
/// already loaded, instead of loading it again in the background.
template <typename Value>
[[nodiscard]] std::shared_future<Value> makeReadyFuture(Value value)
{
    std::promise<Value> promise;
    promise.set_value(std::move(value));
    return promise.get_future().share();
}

} // namespace rtype::core
