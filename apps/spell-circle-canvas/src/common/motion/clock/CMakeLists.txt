# The clock: a pausable, time-scalable, stall-clamped frame clock
# driving a Ticker that steps a choreograph::Timeline plus arbitrary
# steppables, derives Outputs through a bound chain (hence it links the
# bind leaf), and reports whether more frames are needed.
add_library(SigilMotionClock STATIC
  FrameClock.cpp
  Ticker.cpp
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/clock/FrameClock.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/clock/Ticker.h
)
target_compile_features(SigilMotionClock PUBLIC cxx_std_20)
target_include_directories(SigilMotionClock PUBLIC ${SIGIL_MOTION_INCLUDE_DIR})
target_link_libraries(SigilMotionClock PUBLIC
  SigilMotionBind choreograph::choreograph)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
add_executable(motion_clock_test test/ClockTest.cpp)
target_include_directories(motion_clock_test PRIVATE
  ${SIGIL_MOTION_TEST_SUPPORT_DIR})
target_link_libraries(motion_clock_test PRIVATE SigilMotionClock GTest::gtest_main)
add_test(NAME motion_clock_test COMMAND motion_clock_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(motion_clock_bench bench/ClockBench.cpp)
target_link_libraries(motion_clock_bench PRIVATE SigilMotionClock
  benchmark::benchmark)
add_dependencies(benches motion_clock_bench)
