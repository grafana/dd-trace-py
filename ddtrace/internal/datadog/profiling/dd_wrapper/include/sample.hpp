#pragma once

/* Pyroscope patch: Datadog::Sample is Pyroscope::Sample.
 *
 * Upstream this header declares the libdatadog-backed Datadog::Sample -- a
 * vector of ddog_prof_Location2 plus a vector of ddog_prof_Label2 and a
 * values array, exported via ddog_prof_Profile_add2 -- alongside
 * intern_string, intern_function, their ddog_prof_*Id2 handle typedefs, and
 * the internal::StringArena that backs label-value copies.
 *
 * Pyroscope has one Sample shim shared by every profiler in this extension.
 * It carries interned string ids from the Rust-backed string table rather than
 * Profiles Dictionary handles, and has no label storage at all, so the arena
 * has nothing to hold. See cpp/pyroscope/Pyroscope.h, which documents the
 * differences method by method. */

#include "Pyroscope.h"
#include "profiler_stats.hpp"

#include <optional>
#include <string_view>

namespace Datadog {

using Sample = Pyroscope::Sample;
using string_id = Pyroscope::string_id;

// Pyroscope patch: never nullopt; a failed intern yields index 0.
inline std::optional<string_id>
intern_string(std::string_view s)
{
    return Pyroscope::intern_utf8_string(s);
}

} // namespace Datadog
