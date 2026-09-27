# The animation values: Tween and animate(), the one description of a
# motion — readable at any time with no engine, so it reads the same in a
# describe, in a bake and in a test — phase(), quantizeTime()
# and the Oscillator; Animatable<T>, the one noun for a value that changes
# over time, which can hold a shaped binding (hence it links the bind
# leaf) and a staggered tween (hence the schedule). What RUNS a value is
# the clock's, above this feature: nothing here reads a clock.
#
# It links SigilCoreComparable for the field pin the comparators over
# these values sit under, and SigilCoreCompute for the shaped curves.
# Those are edges OUT of this library, and both targets are header-only
# leaves over the standard library (Boost.PFR aside): no kernel, no
# device, nothing that draws.
sigil_library(SigilMotionValues
  HEADERS Animatable.h Interpolate.h Oscillator.h Spring.h Tween.h Time.h
  INTERFACE SigilMotionBind SigilMotionSchedule SigilMotionTime SigilCoreComparable
         SigilCoreCompute)

# Links the values alone, and nothing that
# draws, so a link edge that pulled a drawing library in would show up
# as a build failure here.
sigil_test(motion_test
  SOURCES test/ValuesTest.cpp
          test/OscillatorTest.cpp test/SpringTest.cpp
  LIBRARIES SigilMotionValues)

sigil_bench(motion_bench
  SOURCES bench/ValuesBench.cpp
  LIBRARIES SigilMotionValues)
