#pragma once

// Private to libs/network: the logger of the library, for the files that do not need Asio.

#include "rtype/log/Logger.hpp"

namespace rtype::network::detail
{

/// Logger of the network library.
const rtype::log::Logger &logger();

} // namespace rtype::network::detail
