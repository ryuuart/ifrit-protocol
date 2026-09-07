# SigilCore — where work runs

The chapter on the two ways a library hands work to more than one thread:
one parallel for over the task runtime, taking a count, a grain and a body,
so the runtime is named in one file of this repository and in no header of
it; and beside it a fan-out of its own threads for calls that BLOCK on a
disk or a server, which must not sit on the workers a compute range shares.
`README.md` beside this file is the library.

| header | holds |
|--------|-------|
| `schedule/Parallel.h` | `schedule::parallelFor(count, grain, body)` over contiguous chunks and `schedule::parallelForEach(items, grain, body)` over a range's elements |
| `schedule/ConcurrentIo.h` | `schedule::concurrentIo(count \| items, body)` — one blocking call per item, off the task runtime — and `schedule::concurrentIoWidth()`, how many of them run at once |

`<sigilcore/schedule/Schedule.h>` includes both.

## A grain, and nothing else

`schedule::parallelFor(count, grain, body)` divides `[0, count)` into
contiguous chunks and calls `body(first, last)` on each; the chunks are
disjoint and together cover the range exactly once. The grain is a COUNT OF
ITEMS — how many are worth handing to one worker, which follows from what
one item costs — and it is written where the body is written, because that
is the only place the cost of an item is known. A body that touches one
float per item takes a large grain; a body that compiles a program per item
takes a grain of one.

The grain is also the whole of the small-range rule: a count no larger than
one grain IS one chunk and runs on the calling thread, with no task created
and no second constant that could disagree with the first. What a caller
must not do is put a measured number here and call it settled — a number
that only a benchmark could falsify belongs in a ledger, and the grain that
survives in the code is the one that says what an item costs.

```cpp
#include <sigilcore/schedule/Parallel.h>
using namespace sigil::core;

// A pass over point lanes: cheap per item, so a worker takes many.
schedule::parallelFor(count, kLaneGrain, [&](size_t first, size_t last) {
  for (size_t i = first; i != last; ++i) values[i] = displace(values[i]);
});

// One program compiled per element: expensive per item, so a worker
// takes one.
schedule::parallelForEach(work, 1, [&](const Request& request) {
  compile(request);
});
```

## A call that blocks is not a chunk of work

A read from a disk, a fetch from a server or a wait on another thread's
lock spends nearly all of its time waiting for something that is not a
core. Run through the parallel for, one such call holds a worker of the one
pool every parallel range in the process shares, and a handful of them
stalls all of them — including ranges written nowhere near the fetch.
`schedule::concurrentIo` is the other seam: one blocking call per item, on
threads of its own.

```cpp
#include <sigilcore/schedule/ConcurrentIo.h>

schedule::concurrentIo(pending.size(), [&](size_t i) {
  pending[i].fetched = fetch(pending[i].uri);   // waits on a disk or a server
});
```

Its threads last exactly as long as the call: the fan-out starts them when
it is asked and joins every one before it returns, so nothing is parked
between calls, nothing has to be shut down at exit, and a process that
never fetches has no thread for it. Starting a thread per helper per call
is the price, and it is small beside the wait that motivated the call —
which is also why this seam is wrong for short compute chunks.
`concurrentIoWidth()` is how many run at once, and it is deliberately
larger than the core count: these threads are waiting rather than
computing. A body that throws does not abandon the batch — every item is
still handed out and every thread still joined, and the first exception is
rethrown to the caller afterwards.
