# The colour feature: the colour value that uploads as one float4, its
# packed spelling, the OKLab and OKLCH round trips, the ramp as a value,
# the harmonies read around a hue, the dither threshold and the table a
# run of pixels is made of. The leaf of this library — the core's
# Params.h includes it, so it links nothing of this project's and no
# renderer; the standard library alone. That is why the extraction takes
# PIXELS rather than an image: the crossing from a decoded picture needs
# a renderer, and it lives with the one that owns the picture.
sigil_library(SigilMaterialColor
  SOURCES Color.cpp Dither.cpp Extract.cpp Harmony.cpp Ramp.cpp
  HEADERS Color.h Dither.h Extract.h Harmony.h Ramp.h)

# test/ColorTest.cpp is compiled into material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/ColorBench.cpp
  LIBRARIES SigilMaterialColor)
