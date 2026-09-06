# The mapping value: domain, range, transform, and the tick ladder read
# off it. Standard library only — a colour range is the caller's own
# interpolator read at the scale's unit position, so no colour type
# reaches this target.
sigil_library(SigilDataScale
  SOURCES Scale.cpp
  HEADERS Scale.h)

sigil_test(data_test
  SOURCES test/ScaleTest.cpp
  LIBRARIES SigilDataScale)

sigil_bench(data_bench
  SOURCES bench/ScaleBench.cpp
  LIBRARIES SigilDataScale)
