# The clock: the Engine every animation, timeline and timer runs on and
# the frame that moves it — the wall clock or a stated step, under a
# policy and a budget — and, in advanced/, the held motions and lanes a
# retained host (a reconciler) runs a description's values through. It
# links the values, because what it runs is an Animatable and a Tween. A
# timer is taken through the prefix call, so one that reads neither the
# delta nor the time names neither — hence the callable leaf.
sigil_library(SigilMotionClock
  SOURCES Engine.cpp Held.cpp
  HEADERS Engine.h
          ${CMAKE_CURRENT_SOURCE_DIR}/../include/sigilmotion/advanced/ClockPolicy.h
          ${CMAKE_CURRENT_SOURCE_DIR}/../include/sigilmotion/advanced/Held.h
  PUBLIC SigilMotionValues SigilMotionTime SigilCoreCallable)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/ClockTest.cpp test/EngineTest.cpp test/HeldTest.cpp test/LanesTest.cpp
  LIBRARIES SigilMotionClock)

sigil_bench(motion_bench
  SOURCES bench/ClockBench.cpp
  LIBRARIES SigilMotionClock)
