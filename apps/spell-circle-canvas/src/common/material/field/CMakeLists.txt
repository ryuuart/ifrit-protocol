# The field feature: surfaces evaluated per pixel — the halftone ramp,
# Perlin noise through a leaf, luminance grain, the ripple. Links the
# texture feature for the leaf seam.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialField
  SOURCES Field.cpp
  HEADERS Field.h
  PUBLIC SigilMaterialTexture SigilMaterialColor)
sigil_shader_sources(SigilMaterialField
  DIR ${_shaders} NAMESPACE sigil::material::field)

# The shader directory is named for the case that asks whether the
# embedded table holds every file in it.
sigil_test(material_field_test
  SOURCES test/FieldTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_FIELD_SHADER_DIR="${_shaders}"
  LIBRARIES SigilMaterialField SigilMaterialSkia)

sigil_bench(material_field_bench
  SOURCES bench/FieldBench.cpp
  LIBRARIES SigilMaterialField SigilMaterialSkia)
