#pragma once

/** @file
 * @ingroup core-schedule
 *
 * WORK THAT BLOCKS, OFF THE COMPUTE THREADS.
 *
 * A read from a disk, a fetch from a server, a wait on a device spends
 * nearly all of its time waiting for something that is not a core, and
 * on the task runtime <sigilcore/schedule/Parallel.h> divides ranges
 * over it would hold a worker of that one shared pool for the whole of
 * its wait. So blocking calls get their own threads here.
 *
 * THE THREADS LAST EXACTLY AS LONG AS THE CALL: a fan-out starts its
 * helpers when it is asked and joins every one before it returns, so
 * nothing is parked between calls and nothing is shut down at exit.
 * `concurrentIoWidth()` is how many run at once, and it is NOT the core
 * count — these threads wait rather than compute.
 */

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>

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

/** RUNS @p body ONCE PER INDEX of [0, @p count), at most
 *  `concurrentIoWidth()` of them at a time, and returns when every one
 *  has finished. `body(index)` is called on a thread that is NOT the
 *  task runtime's, which is what makes it the right home for a call
 *  that blocks; indices are handed out in no particular order, each
 *  exactly once, and the calling thread takes a share of them.
 *  @trap A body that throws does not abandon the batch: every index is
 *  still handed out and the first exception is rethrown here. */
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
