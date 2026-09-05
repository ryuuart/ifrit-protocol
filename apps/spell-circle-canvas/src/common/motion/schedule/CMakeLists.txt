# The schedule: how a run of N units share out one master progress. The
# Spread spec, the five orderings its delays are dealt in, and the
# Cascade that resolves the two against the counts a frame actually has.
#
# It links the bind leaf for `easeEqual` — a spread's distribution curve
# compares under the same rule every other curve in the library does —
# and NOT the clock. A cascade is a pure function of a master float in
# [0, 1] and two integer counts, which is what lets a text engine drive
# it from a track's progress, a set from a lane, and a study from a bare
# wrapping phase. A link edge to the clock would be the first step to
# something in here reading time for itself.
#
# The two core edges out of this library are foundational leaves:
# SigilCoreCompute supplies the mixer the scattered ordering ranks with, so a
# seeded permutation is the same permutation everywhere in the tree, and
# SigilCoreComparable supplies the field pin the spec's comparator sits under.
find_package(boost_unordered CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)
sigil_library(SigilMotionSchedule
  SOURCES Cascade.cpp Order.cpp Spread.cpp
  HEADERS Cascade.h Order.h Schedule.h Spread.h
  PUBLIC SigilMotionBind SigilCoreCompute SigilCoreComparable
         choreograph::choreograph
  PRIVATE Boost::unordered)

# Links only the library it exercises — no clock, so a schedule that
# started reading time for itself would show up as a build failure here.
sigil_test(motion_schedule_test
  SOURCES test/ScheduleTest.cpp
  LIBRARIES SigilMotionSchedule)

sigil_bench(motion_schedule_bench
  SOURCES bench/ScheduleBench.cpp
  LIBRARIES SigilMotionSchedule)
