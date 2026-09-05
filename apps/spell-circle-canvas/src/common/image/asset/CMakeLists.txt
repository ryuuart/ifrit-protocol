# The asset leaf: the decoded document (frames, durations, playback) and
# the Skia-codec decode of PNG, JPEG, WebP, GIF and AVIF (stills and
# animations) into premultiplied SkImage frames ready for any SkCanvas.
# Skia only.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilImageAsset
  SOURCES ImageAsset.cpp
  HEADERS ImageAsset.h
  PUBLIC unofficial::skia::skia)

sigil_test(image_asset_test
  SOURCES test/AssetTest.cpp
  LIBRARIES SigilImageAsset)
