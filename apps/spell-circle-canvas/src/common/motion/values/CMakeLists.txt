# The animation values: Transition and the ease:: house curves, the
# animate()/from()/to()/through() keyframe builders, ramp(), phase(),
# quantizeTime() and Animatable<T>, the property slot that holds any one
# of them or a shaped binding (hence it links the bind leaf) — the held
# motion an animatable runs as, which is why this feature links the
# clock: a value that is MOVING is moving on a ticker — and the lanes a
# host retargets those motions through, which name a host's storage
# enumeration and nothing else about it.
#
# It links SigilCoreComparable for the field pin the comparators over
# these values sit under. That is an edge OUT of this library, and the
# one target it reaches is a header-only leaf over the standard library
# and Boost.PFR: no kernel, no device, nothing that draws.
# <sigilmotion/Animation.h> rides here as the umbrella over this feature
# and bind.
find_package(choreograph CONFIG REQUIRED)
sigil_library(SigilMotionValues
  SOURCES Animated.cpp Transition.cpp
  HEADERS ../Animation.h Animatable.h Animated.h Keyframes.h Lanes.h
          Spring.h Time.h Transition.h Values.h
  PUBLIC SigilMotionBind SigilMotionClock SigilCoreComparable
         choreograph::choreograph)

# Links the values and the clock that drives them, and nothing that
# draws, so a link edge that pulled a drawing library in would show up
# as a build failure here.
sigil_test(motion_values_test
  SOURCES test/ValuesTest.cpp test/AnimatedTest.cpp test/LanesTest.cpp
          test/SpringTest.cpp
  LIBRARIES SigilMotionValues SigilMotionClock)

sigil_bench(motion_values_bench
  SOURCES bench/ValuesBench.cpp
  LIBRARIES SigilMotionValues)
