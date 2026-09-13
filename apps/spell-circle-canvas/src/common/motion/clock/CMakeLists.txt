# The clock: a pausable, time-scalable, stall-clamped frame clock
# driving a Ticker that steps a choreograph::Timeline plus arbitrary
# steppables, derives Outputs through a bound chain (hence it links the
# bind leaf), and reports whether more frames are needed. A steppable is
# taken through the prefix call, so one that reads neither the delta nor
# the clock names neither — hence the callable leaf.
find_package(choreograph CONFIG REQUIRED)
sigil_library(SigilMotionClock
  SOURCES FrameClock.cpp Ticker.cpp
  HEADERS FrameClock.h Ticker.h
  PUBLIC SigilMotionBind SigilCoreCallable choreograph::choreograph)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/ClockTest.cpp
  LIBRARIES SigilMotionClock)

sigil_bench(motion_bench
  SOURCES bench/ClockBench.cpp
  LIBRARIES SigilMotionClock)
