# The difference leaf: how many pixels two pictures of one size disagree
# on, the widest gap on any one channel and where it stands. It is what a
# test holding a render against a committed baseline reads, in whichever
# library the render comes from. Skia only, and CPU only: it is a question
# about pixels somebody already has.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilImageDifference
  SOURCES Difference.cpp
  HEADERS Difference.h
  PUBLIC unofficial::skia::skia)

sigil_test(image_test
  SOURCES test/DifferenceTest.cpp
  LIBRARIES SigilImageDifference)

# Identical pictures against pictures that differ in every row, per
# megapixel.
sigil_bench(image_bench
  SOURCES bench/DifferenceBench.cpp
  LIBRARIES SigilImageDifference)
