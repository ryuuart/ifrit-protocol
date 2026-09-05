# The bind leaf: bind(), the Bound chain builder, the BoundFloat
# evaluator it fills, the value-noise field behind wiggle(), and the two
# comparators an identity prune reads a curve and a shaped binding
# through — here because the record this feature owns is the lowest thing
# in the library that carries a curve.
#
# Choreograph, cmath and integers, plus SigilCoreComparable for the field
# pin the shaped-binding comparator sits under: an edge OUT of this
# library, to a header-only leaf over the standard library and Boost.PFR.
sigil_library(SigilMotionBind
  SOURCES Bound.cpp BoundFloat.cpp WiggleNoise.cpp
  HEADERS Bind.h Bound.h BoundFloat.h Curve.h WiggleNoise.h
  PUBLIC SigilCoreComparable choreograph::choreograph)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_bind_test
  SOURCES test/BindTest.cpp
  LIBRARIES SigilMotionBind)

sigil_bench(motion_bind_bench
  SOURCES bench/BindBench.cpp
  LIBRARIES SigilMotionBind)
