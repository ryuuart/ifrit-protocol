# The animation values: Tween and animate(), the one description of a
# motion — readable at any time with no engine, so it reads the same in a
# describe, in a bake and in a test — Transition, phase(), quantizeTime()
# and the Oscillator; Animatable<T>, the one noun for a value that changes
# over time, which can hold a shaped binding (hence it links the bind
# leaf); the held motion an animatable runs as, which is why this feature
# links the clock: a value that is MOVING is moving on a ticker; and the
# lanes a host retargets those motions through, which name a host's
# storage enumeration and nothing else about it.
#
# It links SigilCoreComparable for the field pin the comparators over
# these values sit under, and SigilCoreCompute for the shaped curves.
# Those are edges OUT of this library, and both targets are header-only
# leaves over the standard library (Boost.PFR aside): no kernel, no
# device, nothing that draws.
sigil_library(SigilMotionValues
  SOURCES Animated.cpp Transition.cpp
  HEADERS Animatable.h Animated.h Lanes.h
          Oscillator.h Spring.h Tween.h Time.h Transition.h
  PUBLIC SigilMotionBind SigilMotionClock SigilMotionSchedule SigilCoreComparable
         SigilCoreCompute)

# Links the values and the clock that drives them, and nothing that
# draws, so a link edge that pulled a drawing library in would show up
# as a build failure here.
sigil_test(motion_test
  SOURCES test/ValuesTest.cpp test/AnimatedTest.cpp test/LanesTest.cpp
          test/OscillatorTest.cpp test/SpringTest.cpp
  LIBRARIES SigilMotionValues SigilMotionClock)

sigil_bench(motion_bench
  SOURCES bench/ValuesBench.cpp
  LIBRARIES SigilMotionValues)
