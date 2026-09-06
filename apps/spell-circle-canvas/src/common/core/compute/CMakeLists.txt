# SigilCoreCompute — the arithmetic several libraries have to agree on to
# the bit: the seeded mixers a jitter draws from, the folds a cache key is
# accumulated with, the interval algebra every consumer that answers
# "where on this thing" in runs arrives at, and the shaped curve an
# easing, a colour ramp and a keyed track all reshape a unit position
# with. The standard library is the whole of its dependencies — no glm,
# no Skia, no kernel, no animation runtime — so a shader's CPU twin, a
# point cook, a text cache, a colour leaf and a resource store can all
# reach the same body. Header-only, hence an INTERFACE target: there is
# no archive because every function is inline by nature.
sigil_library(SigilCoreCompute)

# The outputs are the contract — a second implementation of one of these
# bodies is correct only when it answers the same numbers — so the test
# pins them: exact words, and floats compared as bits. Links only the
# library it exercises.
sigil_test(core_test
  SOURCES test/ChanceTest.cpp test/CurveTest.cpp test/FieldTest.cpp
          test/HashTest.cpp test/IntervalsTest.cpp test/NoiseTest.cpp
  LIBRARIES SigilCoreCompute)

sigil_bench(core_bench
  SOURCES bench/ChanceBench.cpp bench/ComputeBench.cpp bench/FieldBench.cpp
  LIBRARIES SigilCoreCompute)
