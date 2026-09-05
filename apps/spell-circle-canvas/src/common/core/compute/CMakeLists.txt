# SigilCoreCompute — the arithmetic several libraries have to agree on to
# the bit: the seeded mixers a jitter draws from, the folds a cache key is
# accumulated with, and the interval algebra every consumer that answers
# "where on this thing" in runs arrives at. The standard library is the whole of its
# dependencies — no glm, no Skia, no kernel — so a shader's CPU twin, a
# point cook, a text cache and a resource store can all reach the same
# body. Header-only, hence an INTERFACE target: there is no archive
# because every function is inline by nature.
sigil_library(SigilCoreCompute)

# The outputs are the contract — a second implementation of one of these
# bodies is correct only when it answers the same numbers — so the test
# pins them: exact words, and floats compared as bits. Links only the
# library it exercises.
sigil_test(core_compute_test
  SOURCES test/HashTest.cpp test/IntervalsTest.cpp test/NoiseTest.cpp
  LIBRARIES SigilCoreCompute)

sigil_bench(core_compute_bench
  SOURCES bench/ComputeBench.cpp
  LIBRARIES SigilCoreCompute)
