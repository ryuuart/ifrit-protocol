# The signed-distance surfaces: three recipes, every style dial a uniform.
# Links the core alone.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialSdf
  SOURCES Sdf.cpp
          ${_shaders}/Circle.sksl ${_shaders}/RoundBox.sksl
          ${_shaders}/Star.sksl ${_shaders}/Style.sksl
  HEADERS Sdf.h
  PUBLIC SigilMaterialCore SigilMaterialColor
  PRIVATE SigilIOHub)
target_compile_definitions(SigilMaterialSdf
  PRIVATE SIGIL_MATERIAL_SDF_SHADER_DIR="${_shaders}")

# The test shades, so it links the Skia backend beside the feature.
sigil_test(material_sdf_test
  SOURCES test/SdfTest.cpp
  LIBRARIES SigilMaterialSdf SigilMaterialSkia)

sigil_bench(material_sdf_bench
  SOURCES bench/SdfBench.cpp
  LIBRARIES SigilMaterialSdf SigilMaterialSkia)
