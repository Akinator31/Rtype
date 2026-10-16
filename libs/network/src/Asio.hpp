#pragma once

// Private to libs/network: the only place where Asio is included, and where our types are converted to its
// types.

#include "NetworkLogger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"

#include <asio.hpp>

namespace rtype::network
{

namespace detail
{

/// Gives the sockets the Asio context of a loop.
struct EventLoopAccess {
    static asio::io_context &context(EventLoop &loop);
};

asio::ip::address toAsio(const Address &address);
Address fromAsio(const asio::ip::address &address, std::uint16_t port);

template <typename Endpoint>
Endpoint toEndpoint(const Address &address)
{
    return Endpoint(toAsio(address), address.port());
}

template <typename Endpoint>
Address fromEndpoint(const Endpoint &endpoint)
{
    return fromAsio(endpoint.address(), endpoint.port());
}

} // namespace detail

} // namespace rtype::network
