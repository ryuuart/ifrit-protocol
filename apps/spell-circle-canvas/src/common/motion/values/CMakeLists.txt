# The animation values: Transition, the
# animate()/from()/to()/through() keyframe builders, ramp(), phase(),
# quantizeTime(), the Oscillator and the keyed Sequence — the two values
# that are functions of a time and of nothing else, so they read the
# same in a describe, in a bake and in a test — and Animatable<T>, the
# property slot that holds any one
# of them or a shaped binding (hence it links the bind leaf) — the held
# motion an animatable runs as, which is why this feature links the
# clock: a value that is MOVING is moving on a ticker — and the lanes a
# host retargets those motions through, which name a host's storage
# enumeration and nothing else about it.
#
# It links SigilCoreComparable for the field pin the comparators over
# these values sit under, and SigilCoreCompute for the shaped curve a
# keyed step's segment is drawn with. Those are edges OUT of this
# library, and both targets are header-only leaves over the standard
# library (Boost.PFR aside): no kernel, no device, nothing that draws.
# <sigilmotion/Animation.h> rides here as the umbrella over this feature
# and bind.
find_package(choreograph CONFIG REQUIRED)
sigil_library(SigilMotionValues
  SOURCES Animated.cpp Transition.cpp
  HEADERS ../Animation.h Animatable.h Animated.h Keyframes.h Lanes.h
          Oscillator.h Sequence.h Spring.h Time.h Transition.h Values.h
  PUBLIC SigilMotionBind SigilMotionClock SigilCoreComparable
         SigilCoreCompute choreograph::choreograph)

# Links the values and the clock that drives them, and nothing that
# draws, so a link edge that pulled a drawing library in would show up
# as a build failure here.
sigil_test(motion_test
  SOURCES test/ValuesTest.cpp test/AnimatedTest.cpp test/LanesTest.cpp
          test/OscillatorTest.cpp test/SequenceTest.cpp test/SpringTest.cpp
  LIBRARIES SigilMotionValues SigilMotionClock)

sigil_bench(motion_bench
  SOURCES bench/ValuesBench.cpp
  LIBRARIES SigilMotionValues)
