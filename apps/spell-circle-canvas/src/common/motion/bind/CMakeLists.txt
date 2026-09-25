# The bind feature: bind(), the Bound chain builder, the BoundFloat
# evaluator it fills, the value-noise field behind wiggle(), and the
# comparator an identity prune reads a shaped binding through. It links
# the ease feature for the curves a binding's map and wave hold.
#
# Choreograph, cmath and integers, plus one edge OUT of this library, to a
# header-only leaf over the standard library: SigilCoreComparable for the
# field pin the shaped-binding comparator sits under.
find_package(choreograph CONFIG REQUIRED)
sigil_library(SigilMotionBind
  SOURCES Bound.cpp BoundFloat.cpp WiggleNoise.cpp
  HEADERS Bind.h Bound.h BoundFloat.h WiggleNoise.h
  PUBLIC SigilMotionEase SigilCoreComparable choreograph::choreograph)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/BindTest.cpp test/BoundFloatTest.cpp
          test/CurveComparatorTest.cpp test/WiggleNoiseTest.cpp
  LIBRARIES SigilMotionBind)

sigil_bench(motion_bench
  SOURCES bench/BindBench.cpp
  LIBRARIES SigilMotionBind)
