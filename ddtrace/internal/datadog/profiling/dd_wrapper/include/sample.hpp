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

#include <string_view>
#include <vector>

namespace Datadog {

namespace internal {

// StringArena holds copies of strings we need while building samples.
// StringArena is intended to amortize allocations, so that in the common
// case we can just do a memcpy for each string we want to copy rather than
// a new allocation for each string.
//
// We need to make copies right now because we don't have strong guarantees
// that the strings we get (from Python, Cython, C++, etc) are alive the
// whole time we build samples.
struct StringArena
{
  private:
    // Default size, in bytes, of each Chunk. Frame strings (function names,
    // filenames) are interned directly into libdatadog's ProfilesDictionary,
    // so only label values (thread name, task name, trace type, lock name,
    // etc.) are stored here. Typical total per sample is 20-150 bytes.
    // 256 bytes covers the vast majority of samples; insert() allocates a
    // new chunk for the rare overflow case, so correctness is preserved.
    static constexpr size_t DEFAULT_SIZE = 256;

    // Strings are backed by fixed-size Chunks. The Chunks can't grow, or
    // they'll move and invalidate pointers into the arena. At the same time,
    // they must be dynamically sized at creation because we get arbitrary
    // user-provided strings.
    using Chunk = std::vector<char>;

    // We keep the Chunks for this arena in a vector so we can track them, and
    // free them when the StringArena is deallocated.
    std::vector<Chunk> chunks;

  public:
    StringArena();
    // Clear the backing data of the arena, except for a smaller initial segment.
    // Views returned by insert are invalid after this call.
    void reset();
    // Copies the contents of s into the arena and returns a view of the copy in
    // the arena. The returned view is valid until the next call to reset, or
    // until the arena is destroyed.
    std::string_view insert(std::string_view s);
};

} // namespace internal

} // namespace Datadog
