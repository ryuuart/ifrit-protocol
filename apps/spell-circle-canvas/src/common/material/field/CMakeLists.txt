# The field feature: surfaces evaluated per pixel — the halftone ramp,
# Perlin noise through a leaf, luminance grain, the ripple. Links the
# texture feature for the leaf seam.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialField
  SOURCES Field.cpp
          ${_shaders}/CrtOverlay.sksl ${_shaders}/Grain.sksl
          ${_shaders}/HalftoneRamp.sksl ${_shaders}/Noise.sksl
          ${_shaders}/Ripple.sksl
  HEADERS Field.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilIOHub)
target_compile_definitions(SigilMaterialField
  PRIVATE SIGIL_MATERIAL_FIELD_SHADER_DIR="${_shaders}")

sigil_test(material_field_test
  SOURCES test/FieldTest.cpp
  LIBRARIES SigilMaterialField SigilMaterialSkia)

sigil_bench(material_field_bench
  SOURCES bench/FieldBench.cpp
  LIBRARIES SigilMaterialField SigilMaterialSkia)
