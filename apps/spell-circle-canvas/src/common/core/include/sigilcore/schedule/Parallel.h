#pragma once

/** @file
 * @ingroup core-schedule
 *
 * ONE PARALLEL FOR, over the task runtime the process already carries.
 *
 * A range of independent items is divided into contiguous chunks and the
 * chunks are run on whatever workers that runtime has. What crosses this
 * seam is a count, a grain and a body — the runtime is named nowhere in
 * this header, so a consumer neither includes it nor links it.
 *
 * NOTHING HERE IS FOR WORK THAT BLOCKS. A body that waits on a disk, on
 * a socket, or on a lock some other thread holds occupies a worker of
 * the one pool every parallel range in the process shares, and a
 * handful of such waits stalls all of them. Blocking work goes through
 * <sigilcore/schedule/ConcurrentIo.h>, which has its own threads.
 */

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>

/** WHERE INDEPENDENT WORK RUNS. One parallel for over the task runtime
 *  the process already carries, taking a count, a grain and a body — so
 *  the runtime is named in one file of this repository and in no header
 *  of it — and beside it a fan-out of its own threads for calls that
 *  BLOCK on a disk or a server, which must not sit on the workers a
 *  compute range shares. */
namespace sigil::core::schedule {

namespace detail {

/** The one shape that crosses into the task runtime: a chunk body reached
 *  through a pointer, so nothing is instantiated where the runtime's
 *  headers are and no consumer of this header sees them. */
using ChunkBody = void (*)(void*, size_t, size_t);

/** [0, @p count) in chunks of at least @p grain items, @p run called with
 *  @p body and each chunk. */
void overChunks(size_t count, size_t grain, void* body, ChunkBody run);

}  // namespace detail

/** RUNS @p body OVER [0, @p count) AS CONTIGUOUS CHUNKS, on as many
 *  workers as the runtime gives: `body(first, last)` over disjoint
 *  half-open ranges, in no particular order, covering the whole exactly
 *  once. @p grain is how many items are worth handing to one worker,
 *  zero meaning one item; a count no larger than the grain runs on the
 *  calling thread.
 *  @trap Every chunk may run on a different thread, so a body writes
 *  only what its own range names; an exception leaves the range partly
 *  run and is rethrown here. */
template <std::unsigned_integral Index, class Body>
void parallelFor(Index count, size_t grain, Body&& body) {
  auto chunk = [&body](size_t first, size_t last) {
    body(static_cast<Index>(first), static_cast<Index>(last));
  };
  detail::overChunks(static_cast<size_t>(count), grain, &chunk,
                     [](void* held, size_t first, size_t last) {
                       (*static_cast<decltype(chunk)*>(held))(first, last);
                     });
}

/** Run @p body over each element of @p items, in chunks of @p grain
 *  elements. `body(element)` per element, under `parallelFor`'s rules. */
template <std::ranges::random_access_range Range, class Body>
void parallelForEach(Range&& items, size_t grain, Body&& body) {
  auto first = std::ranges::begin(items);
  parallelFor(static_cast<size_t>(std::ranges::size(items)), grain,
              [&](size_t from, size_t to) {
                for (size_t i = from; i != to; ++i) body(first[i]);
              });
}

}  // namespace sigil::core::schedule
