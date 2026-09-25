# The schedule: `stagger()`, a value that differs per child — a delay that
# steps from one sibling to the next, a range spread across them, a table
# of cues — resolved from a child's place among its siblings; and the
# Schedule a Timing resolves into against the counts a frame actually has.
#
# It links the ease leaf for `easeEqual` — a stagger's curve compares under
# the same rule every other curve in the library does — and the time unit,
# and NOT the clock. A schedule is a pure function of a master float in
# [0, 1] and two integer counts, which is what lets a text engine drive it
# from a track's progress and a study from a bare wrapping phase. A link
# edge to the clock would be the first step to something in here reading
# time for itself.
#
# The two core edges out of this library are foundational leaves:
# SigilCoreCompute supplies the mixer the scattered ordering ranks with, so a
# seeded permutation is the same permutation everywhere in the tree, and
# SigilCoreComparable supplies the field pin the comparators sit under.
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilMotionSchedule
  SOURCES Schedule.cpp Stagger.cpp
  HEADERS Schedule.h Stagger.h
  PUBLIC SigilMotionEase SigilMotionTime SigilCoreCompute SigilCoreComparable
  PRIVATE Boost::unordered)

# Links only the library it exercises — no clock, so a schedule that
# started reading time for itself would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/ScheduleTest.cpp
  LIBRARIES SigilMotionSchedule)

sigil_bench(motion_bench
  SOURCES bench/ScheduleBench.cpp
  LIBRARIES SigilMotionSchedule)
