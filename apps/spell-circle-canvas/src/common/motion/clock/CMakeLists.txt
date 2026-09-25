# The clock: a pausable, time-scalable, stall-clamped frame clock, the
# policy that says whether the wall, a caller's steps or nobody moves it,
# and a Ticker that steps the motions a host started plus arbitrary
# steppables and reports whether more frames are needed. A steppable is
# taken through the prefix call, so one that reads neither the delta nor
# the clock names neither — hence the callable leaf.
sigil_library(SigilMotionClock
  SOURCES ClockPolicy.cpp FrameClock.cpp Ticker.cpp
  HEADERS ClockPolicy.h FrameClock.h Ticker.h
  PUBLIC SigilMotionTime SigilCoreCallable)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/ClockPolicyTest.cpp test/ClockTest.cpp
  LIBRARIES SigilMotionClock)

sigil_bench(motion_bench
  SOURCES bench/ClockBench.cpp
  LIBRARIES SigilMotionClock)
