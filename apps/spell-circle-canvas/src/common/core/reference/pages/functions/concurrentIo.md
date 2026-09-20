---
kind: function
library: SigilCore
name: concurrentIo
qualified: sigil::core::schedule::concurrentIo
group: Schedule
status: stable
---

# concurrentIo

WORK THAT BLOCKS, OFF THE COMPUTE THREADS.

Runs the body once per index of the range, at most
`sigil::core::schedule::concurrentIoWidth` of them at a time, and
returns when every one has finished. `body(index)` is called on a
thread that is not the task runtime's, which is what makes it the right
home for a call that blocks. Indices are handed out in no particular
order and each is passed exactly once; the calling thread takes a share
of them rather than only waiting. A body writes only what its own index
names. A body that throws does not abandon the rest of the batch: every
index is still handed out, every thread is still joined, and the first
exception raised is rethrown to the caller once they are.

## Why its own threads

A read from a disk, a fetch from a server, a wait on a device: each
spends nearly all of its time waiting for something that is not a core.
Run on the task runtime that `sigil::core::schedule::parallelFor`
divides ranges over, one such call holds a worker of that one shared
pool for the whole of its wait — and a handful of them stall every
parallel range in the process, however far from the fetch that range
was written. So blocking calls get their own threads here, and the two
kinds of concurrency never contend for one pool.

## The threads last exactly as long as the call

A fan-out starts its helpers when it is asked and joins every one of
them before it returns: nothing is parked between calls, nothing has to
be shut down at exit, and a process that never fetches anything never
has a thread for it. What that costs is starting a thread per helper
per call, which is the bargain worth making for work whose whole point
is that it waits — and the reason this is not the seam for short
compute chunks.

## The width is not the core count

These threads wait rather than compute, so having more of them than
there are cores is the point: what a fetch is waiting for makes
progress while the thread is off the processor.
`sigil::core::schedule::concurrentIoWidth` is that number, derived from
the hardware concurrency the machine reports.

## See also

- `schedule/ConcurrentIo.h` — the header: `concurrentIo`,
  `concurrentIoWidth`
- [parallelFor](page:SigilCore/functions/parallelFor) — the seam for
  work that computes
- `SCHEDULE.md` — the chapter both seams are described in
