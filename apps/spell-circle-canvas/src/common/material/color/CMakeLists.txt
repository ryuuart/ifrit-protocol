# The colour feature: the colour value that uploads as one float4, its
# packed spelling, the OKLab and OKLCH round trips, the ramp as a value,
# the harmonies read around a hue, the dither threshold and the table a
# run of pixels is made of. The leaf of this library — the core's
# Parameters.h includes it, so it carries no renderer and nothing that draws.
# Its one link is SigilCoreCompute, header-only over the standard library,
# for the shaped curve a ramp walks its position through: the same value
# an easing and a keyed track read, so a look chosen once reads the same
# everywhere. That is why the extraction takes PIXELS rather than an
# image: the crossing from a decoded picture needs a renderer, and it
# lives with the one that owns the picture.
sigil_library(SigilMaterialColor
  SOURCES Color.cpp Dither.cpp Extract.cpp Harmony.cpp Ramp.cpp
  HEADERS Color.h Dither.h Extract.h Harmony.h Ramp.h
  PUBLIC SigilCoreCompute)

# test/ColorTest.cpp is compiled into material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/ColorBench.cpp
  LIBRARIES SigilMaterialColor)
