# The signed-distance surfaces: three recipes, every style dial a uniform.
# Links the core alone.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialSdf
  SOURCES Sdf.cpp
  HEADERS Sdf.h
  PUBLIC SigilMaterialCore SigilMaterialColor)
sigil_shader_sources(SigilMaterialSdf
  DIR ${_shaders} NAMESPACE sigil::material::sdf)

# The test shades, so it links the Skia backend beside the feature. The
# shader directory is named for the case that asks whether the embedded
# table holds every file in it.
sigil_test(material_sdf_test
  SOURCES test/SdfTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SDF_SHADER_DIR="${_shaders}"
  LIBRARIES SigilMaterialSdf SigilMaterialSkia)

sigil_bench(material_sdf_bench
  SOURCES bench/SdfBench.cpp
  LIBRARIES SigilMaterialSdf SigilMaterialSkia)
