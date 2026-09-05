# The colour feature: the colour value that uploads as one float4, its
# packed spelling, and the OKLab round trip. The leaf of this library —
# the core's Params.h includes it, so it links nothing of this project's
# and no renderer; the standard library alone.
sigil_library(SigilMaterialColor
  SOURCES Color.cpp
  HEADERS Color.h)

# test/ColorTest.cpp is compiled into material_test, at the library root.

sigil_bench(material_color_bench
  SOURCES bench/ColorBench.cpp
  LIBRARIES SigilMaterialColor)
