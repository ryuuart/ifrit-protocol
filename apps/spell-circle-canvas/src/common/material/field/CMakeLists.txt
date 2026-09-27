# The field feature: surfaces evaluated per pixel — the halftone ramp,
# Perlin noise through a leaf, luminance grain, the ripple. The Perlin
# leaf is Skia's own generator bound as a Skia shader leaf, so the Skia
# executor is linked privately: no field header names it.
set(SIGIL_MATERIAL_FIELD_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the field feature's shader modules")
set(_shaders ${SIGIL_MATERIAL_FIELD_SHADER_DIR})
sigil_library(SigilMaterialField
  SOURCES Field.cpp Crt.cpp
  HEADERS Field.h Crt.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilMaterialSkia)
sigil_shader_sources(SigilMaterialField
  DIR ${_shaders} NAMESPACE sigil::material::field)

# test/FieldTest.cpp is compiled into material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/FieldBench.cpp
  LIBRARIES SigilMaterialField SigilMaterialSkia)
