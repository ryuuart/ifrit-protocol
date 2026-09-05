#pragma once

/** @file
 * ONE PARALLEL FOR, over the task runtime the process already carries.
 *
 * A range of independent items is divided into contiguous chunks and the
 * chunks are run on whatever workers that runtime has. What crosses this
 * seam is a count, a grain and a body — the runtime is named nowhere in
 * this header, so a consumer neither includes it nor links it, and there
 * is one place in the tree that decides how a range is split.
 *
 * THE GRAIN IS THE CALLER'S, and it is a count of items rather than a
 * switch. It says how many items are worth handing to one worker, which
 * follows from what ONE ITEM COSTS: the chunk has to be worth more than
 * the handing over. A body that touches one float per item wants a large
 * grain; a body that compiles a program per item wants a grain of one.
 * That number is written where the body is written, because that is the
 * only place the cost of an item is known.
 *
 * The grain is also the whole of the small-range rule. A count no larger
 * than one grain IS one chunk and runs on the calling thread — no task,
 * no worker, and no second constant that could disagree with the first.
 *
 * NOTHING HERE IS FOR WORK THAT BLOCKS. A body that waits on a disk, on a
 * socket, or on a lock some other thread holds occupies a worker of the
 * one pool every parallel range in the process shares, and a handful of
 * such waits stalls all of them. Blocking work goes through
 * <sigilcore/schedule/ConcurrentIo.h>, which has its own threads.
 */

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

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

/** Run @p body over [0, @p count) as contiguous chunks, on as many
 *  workers as the runtime gives.
 *
 *  @p body is called as `body(first, last)` over a half-open range. The
 *  chunks are disjoint, in no particular order, and together they cover
 *  [0, @p count) exactly once; a count of zero calls @p body not at all.
 *  Every chunk may run on a different thread, so a body writes only what
 *  its own range names and reads only what nothing else writes.
 *
 *  @p grain is how many items one worker takes at a time — see the file
 *  comment for how a caller chooses it. A grain of zero is one item. A
 *  count no larger than the grain runs on the calling thread.
 *
 *  An exception a body throws leaves the range partly run and is
 *  rethrown to this caller. */
template <std::unsigned_integral Index, class Body>
void parallelFor(Index count, Index grain, Body&& body) {
  auto chunk = [&body](size_t first, size_t last) {
    body(static_cast<Index>(first), static_cast<Index>(last));
  };
  detail::overChunks(static_cast<size_t>(count), static_cast<size_t>(grain),
                     &chunk, [](void* held, size_t first, size_t last) {
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
