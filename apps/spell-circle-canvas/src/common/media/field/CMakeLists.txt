# The field leaf: a picture's alpha as a coverage mask, and the exact
# Euclidean distance from every pixel to the nearest covered one. It is
# what makes "everything within m of this shape" a disc offset rather than
# a square one, for any raster — an analytic distance function answers only
# for the shapes it is written for. CPU only: it is a question about
# pixels somebody already has.
sigil_library(SigilMediaField
  SOURCES DistanceField.cpp
  HEADERS DistanceField.h
  PUBLIC SigilMediaCore)

sigil_test(media_test
  SOURCES test/DistanceFieldTest.cpp
  LIBRARIES SigilMediaField)
