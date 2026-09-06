# The field leaf: an image's alpha as a coverage mask, and the exact
# Euclidean distance from every pixel to the nearest covered one. It is
# what makes "everything within m of this shape" a disc offset rather than
# a square one, for any raster — an analytic distance function answers only
# for the shapes it is written for. Skia only, and CPU only: it is a
# question about pixels somebody already has.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilImageField
  SOURCES DistanceField.cpp
  HEADERS DistanceField.h
  PUBLIC unofficial::skia::skia)

sigil_test(image_test
  SOURCES test/DistanceFieldTest.cpp
  LIBRARIES SigilImageField)
