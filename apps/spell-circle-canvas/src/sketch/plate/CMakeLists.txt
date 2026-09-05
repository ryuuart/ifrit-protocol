# The headless renderer: the sweep that steps every sketch to its
# declared moment and photographs it, the montage, the comparison of two
# directories of plates, and the thumbnail store a browser fills on
# demand. A plate is a function of the declaration alone and never of
# how fast the machine ran.
sigil_test(sketch_test
  SOURCES test/CompareTest.cpp test/SweepTest.cpp
  LIBRARIES SigilSketch SigilWeavePorts)

sigil_bench(sketch_bench
  SOURCES bench/SweepBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)
