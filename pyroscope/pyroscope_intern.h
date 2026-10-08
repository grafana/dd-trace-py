#pragma once

#include "Pyroscope.h"

#include <optional>
#include <string_view>

namespace Datadog {

// Pyroscope patch: never nullopt; a failed intern yields index 0.
inline std::optional<Pyroscope::string_id>
intern_string(std::string_view s)
{
    return Pyroscope::intern_utf8_string(s);
}

} // namespace Datadog
