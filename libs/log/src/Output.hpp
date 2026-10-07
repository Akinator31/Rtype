#pragma once

// Private to the log library: sends a record to the current sink.

#include "rtype/log/Log.hpp"

namespace rtype::log::detail
{

/// Sends a record to the current sink, one record at a time.
void output(const Record &record);

} // namespace rtype::log::detail
