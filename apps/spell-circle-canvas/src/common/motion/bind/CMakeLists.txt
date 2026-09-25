# The bind feature: `Binding`, the stages a followed number is shaped
# through as fields, the envelopes that are its waveform vocabulary, the
# value-noise field behind its wiggle, and the equality an identity prune
# reads a binding through. It links the ease feature for the curves a
# binding holds.
#
# cmath and integers, plus one edge OUT of this library, to a header-only
# leaf over the standard library: SigilCoreComparable for the field pin
# the binding's equality sits under.
sigil_library(SigilMotionBind
  SOURCES Binding.cpp WiggleNoise.cpp
  HEADERS Binding.h WiggleNoise.h
  PUBLIC SigilMotionEase SigilCoreComparable)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/BindingTest.cpp test/CurveComparatorTest.cpp
          test/WiggleNoiseTest.cpp
  LIBRARIES SigilMotionBind)

sigil_bench(motion_bench
  SOURCES bench/BindBench.cpp
  LIBRARIES SigilMotionBind)
