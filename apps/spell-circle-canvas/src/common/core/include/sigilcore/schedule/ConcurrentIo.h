#pragma once

/** @file
 * WORK THAT BLOCKS, OFF THE COMPUTE THREADS.
 *
 * A read from a disk, a fetch from a server, a wait on a device: each
 * spends nearly all of its time waiting for something that is not a core.
 * Run on the task runtime that <sigilcore/schedule/Parallel.h> divides
 * ranges over, one such call holds a worker of that one shared pool for
 * the whole of its wait — and a handful of them stall every parallel
 * range in the process, however far from the fetch that range was
 * written. So blocking calls get their own threads here, and the two
 * kinds of concurrency never contend for one pool.
 *
 * THE THREADS LAST EXACTLY AS LONG AS THE CALL. A fan-out starts its
 * helpers when it is asked and joins every one of them before it returns:
 * nothing is parked between calls, nothing has to be shut down at exit,
 * and a process that never fetches anything never has a thread for it.
 * What that costs is starting a thread per helper per call, which is the
 * bargain worth making for work whose whole point is that it waits — and
 * the reason this is not the seam for short compute chunks.
 *
 * THE WIDTH IS NOT THE CORE COUNT. These threads wait rather than
 * compute, so having more of them than there are cores is the point: what
 * a fetch is waiting for makes progress while the thread is off the
 * processor. `concurrentIoWidth()` is that number, derived from the
 * hardware concurrency the machine reports.
 */

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

namespace sigil::core::schedule {

/** How many blocking calls this process runs at once. At least one, and
 *  more than the machine has cores. */
size_t concurrentIoWidth();

namespace detail {

/** One blocking item reached through a pointer, so the threading is
 *  compiled once rather than per body. */
using ItemBody = void (*)(void*, size_t);

/** Every index of [0, @p count), each on a thread of the fan-out. */
void overIoItems(size_t count, void* body, ItemBody run);

}  // namespace detail

/** Run @p body once per index of [0, @p count), at most
 *  `concurrentIoWidth()` of them at a time, and return when every one has
 *  finished.
 *
 *  `body(index)` is called on a thread that is not the task runtime's,
 *  which is what makes it the right home for a call that blocks. Indices
 *  are handed out in no particular order and each is passed exactly once;
 *  the calling thread takes a share of them rather than only waiting. A
 *  body writes only what its own index names.
 *
 *  A body that throws does not abandon the rest of the batch: every index
 *  is still handed out, every thread is still joined, and the first
 *  exception raised is rethrown to this caller once they are. */
template <class Body>
  requires std::invocable<Body&, size_t>
void concurrentIo(size_t count, Body&& body) {
  auto item = [&body](size_t index) { body(index); };
  detail::overIoItems(count, &item, [](void* held, size_t index) {
    (*static_cast<decltype(item)*>(held))(index);
  });
}

/** The same over the elements of @p items: `body(element)` per element,
 *  each on a thread of the fan-out. */
template <std::ranges::random_access_range Range, class Body>
void concurrentIo(Range&& items, Body&& body) {
  auto first = std::ranges::begin(items);
  concurrentIo(static_cast<size_t>(std::ranges::size(items)),
               [&](size_t index) { body(first[index]); });
}

}  // namespace sigil::core::schedule
