# SigilCoreCompute — the arithmetic several libraries have to agree on to
# the bit: the seeded mixers a jitter draws from, and the folds a cache
# key is accumulated with. The standard library is the whole of its
# dependencies — no glm, no Skia, no kernel — so a shader's CPU twin, a
# point cook, a text cache and a resource store can all reach the same
# body. Header-only, hence an INTERFACE target: there is no archive
# because every function is inline by nature.
sigil_library(SigilCoreCompute)

# The outputs are the contract, so the test pins them: exact words, and
# floats compared as bits. Links only the library it exercises.
sigil_test(sigilcore_compute_test
  SOURCES test/HashTest.cpp test/NoiseTest.cpp
  LIBRARIES SigilCoreCompute)

sigil_bench(sigilcore_compute_bench
  SOURCES bench/ComputeBench.cpp
  LIBRARIES SigilCoreCompute)
