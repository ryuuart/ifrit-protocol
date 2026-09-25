# The curves: `Easing`, the one type every curve slot in the library
# holds, the named catalogue under `ease::`, the parameterised families,
# and `easeEqual`, the rule two held curves compare by. It is the bottom
# of the library because every other feature holds a curve.
#
# Its one edge out is SigilCoreCompute, the header-only leaf that owns
# the comparable shaped-curve value, because a colour ramp and a keyed
# track reshape a unit position exactly as an easing does and link no
# animation runtime.
sigil_library(SigilMotionEase
  SOURCES Ease.cpp
  HEADERS Ease.h
  PUBLIC SigilCoreCompute)

# Links choreograph beside the library it exercises: the named curves are
# held to the same numbers as the animation runtime's own, so a curve
# swapped for its name here moves no pixel.
find_package(choreograph CONFIG REQUIRED)
sigil_test(motion_test
  SOURCES test/EaseTest.cpp
  LIBRARIES SigilMotionEase choreograph::choreograph)

sigil_bench(motion_bench
  SOURCES bench/EaseBench.cpp
  LIBRARIES SigilMotionEase)
