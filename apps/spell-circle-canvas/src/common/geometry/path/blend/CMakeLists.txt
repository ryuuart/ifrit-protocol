# Shape interpolation over resampled contours — the Illustrator blend.
# The colour interpolation runs in OKLab, whose conversions are
# SigilMaterial's colour leaf; the link is private because no header here
# spells its types.
sigil_library(SigilGeometryPathBlend
  SOURCES Blend.cpp
  HEADERS Blend.h
  PUBLIC SigilGeometryPath
  PRIVATE SigilMaterialColor)

sigil_bench(geometry_path_blend_bench
  SOURCES bench/BlendBench.cpp
  LIBRARIES SigilGeometryPathBlend)
