# SigilCoreSchedule — where independent work runs: one parallel for over
# the task runtime, and a fan-out of plain threads for calls that block on
# a disk or a server. It is a feature of its own rather than part of
# SigilCoreCompute because compute is the arithmetic several libraries
# agree on to the bit and takes the standard library alone, while this
# takes a task runtime and a thread of its own — an executor is not a
# function, and a library that wants one must not acquire the other.
#
# oneTBB is PRIVATE and named in Parallel.cpp alone: every consumer takes
# a count, a grain and a body, so no header in this repository spells a
# task runtime and the runtime can be changed here for all of them.
find_package(TBB CONFIG REQUIRED)
sigil_library(SigilCoreSchedule
  SOURCES ConcurrentIo.cpp Parallel.cpp
  HEADERS ConcurrentIo.h Parallel.h Schedule.h
  PRIVATE TBB::tbb)

# What the seam promises rather than how fast it is: that the chunks cover
# the range exactly once, that the grain alone decides when a range stays
# on its caller, and that the blocking fan-out really is concurrent and
# joins everything it starts.
sigil_test(core_test
  SOURCES test/ConcurrentIoTest.cpp test/ParallelTest.cpp
  LIBRARIES SigilCoreSchedule)

sigil_bench(core_bench
  SOURCES bench/ScheduleBench.cpp
  LIBRARIES SigilCoreSchedule)
