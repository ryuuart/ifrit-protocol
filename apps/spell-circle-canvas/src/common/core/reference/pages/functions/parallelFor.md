---
kind: function
library: SigilCore
name: parallelFor
qualified: sigil::core::schedule::parallelFor
group: Schedule
status: stable
---

# parallelFor

ONE PARALLEL FOR, over the task runtime the process already carries.

A range of independent items is divided into contiguous chunks and the
chunks are run on whatever workers that runtime has. What crosses this
seam is a count, a grain and a body — the runtime is named nowhere in
the header, so a consumer neither includes it nor links it, and there
is one place in the tree that decides how a range is split.

The body is called as `body(first, last)` over a half-open range. The
chunks are disjoint, in no particular order, and together they cover
the whole range exactly once; a count of zero calls the body not at
all. Every chunk may run on a different thread, so a body writes only
what its own range names and reads only what nothing else writes. An
exception a body throws leaves the range partly run and is rethrown to
the caller.

## The grain is the caller's

It is a count of items rather than a switch. It says how many items are
worth handing to one worker, which follows from what ONE ITEM COSTS:
the chunk has to be worth more than the handing over. A body that
touches one float per item wants a large grain; a body that compiles a
program per item wants a grain of one. That number is written where the
body is written, because that is the only place the cost of an item is
known. A grain of zero is one item.

The grain is also the whole of the small-range rule. A count no larger
than one grain IS one chunk and runs on the calling thread — no task,
no worker, and no second constant that could disagree with the first.

## Nothing here is for work that blocks

A body that waits on a disk, on a socket, or on a lock some other
thread holds occupies a worker of the one pool every parallel range in
the process shares, and a handful of such waits stalls all of them.
Blocking work goes through `sigil::core::schedule::concurrentIo`,
which has its own threads.

## See also

- `schedule/Parallel.h` — the header: `parallelFor`
- [concurrentIo](page:SigilCore/functions/concurrentIo) — the seam for
  work that waits
- `SCHEDULE.md` — the chapter both seams are described in
