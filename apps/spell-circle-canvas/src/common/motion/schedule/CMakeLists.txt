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
# The two edges out of this library are the std-only leaves: SigilCoreCompute
# for the mixer the scattered ordering ranks with, so a seeded permutation
# is the same permutation everywhere in the tree, and SigilCoreComparable
# for the field pin the spec's comparator sits under.
add_library(SigilMotionSchedule STATIC
  Cascade.cpp
  Order.cpp
  Spread.cpp
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/schedule/Cascade.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/schedule/Order.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/schedule/Schedule.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/schedule/Spread.h
)
target_compile_features(SigilMotionSchedule PUBLIC cxx_std_20)
target_include_directories(SigilMotionSchedule PUBLIC
  ${SIGIL_MOTION_INCLUDE_DIR})
target_link_libraries(SigilMotionSchedule PUBLIC
  SigilMotionBind SigilCoreCompute SigilCoreComparable
  choreograph::choreograph)

# Links only the library it exercises — no clock, so a schedule that
# started reading time for itself would show up as a build failure here.
add_executable(motion_schedule_test test/ScheduleTest.cpp)
target_link_libraries(motion_schedule_test PRIVATE
  SigilMotionSchedule GTest::gtest_main)
add_test(NAME motion_schedule_test COMMAND motion_schedule_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(motion_schedule_bench bench/ScheduleBench.cpp)
target_link_libraries(motion_schedule_bench PRIVATE SigilMotionSchedule
  benchmark::benchmark)
add_dependencies(benches motion_schedule_bench)
