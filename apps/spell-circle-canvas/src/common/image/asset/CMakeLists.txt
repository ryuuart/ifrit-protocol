# The asset leaf: the decoded document (frames, durations, playback), the
# Skia-codec decode of PNG, JPEG, WebP, GIF and AVIF (stills and
# animations) into premultiplied SkImage frames ready for any SkCanvas,
# and the signature scan that recovers whole images from a container this
# library has no parser for. Skia only.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilImageAsset
  SOURCES ImageAsset.cpp Embedded.cpp
  HEADERS ImageAsset.h Embedded.h
  PUBLIC unofficial::skia::skia)

sigil_test(image_test
  SOURCES test/AssetTest.cpp test/EmbeddedTest.cpp
  LIBRARIES SigilImageAsset)
