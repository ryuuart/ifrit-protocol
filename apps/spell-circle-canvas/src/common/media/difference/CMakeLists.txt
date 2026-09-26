# The difference leaf: how many pixels two pictures of one size disagree
# on, the widest gap on any one channel and where it stands. It is what a
# test holding a render against a committed baseline reads, in whichever
# library the render comes from. CPU only: it is a question about pixels
# somebody already has, read back first where they are not on the CPU.
sigil_library(SigilMediaDifference
  SOURCES Difference.cpp
  HEADERS Difference.h
  PUBLIC SigilMediaCore)

sigil_test(media_test
  SOURCES test/DifferenceTest.cpp
  LIBRARIES SigilMediaDifference)

# Identical pictures against pictures that differ in every row, per
# megapixel.
sigil_bench(media_bench
  SOURCES bench/DifferenceBench.cpp
  LIBRARIES SigilMediaDifference)
