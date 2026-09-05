# The signed-distance surfaces: three recipes, every style dial a uniform.
# Links the core alone.
set(SIGIL_MATERIAL_SDF_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the sdf feature's shader modules")
set(_shaders ${SIGIL_MATERIAL_SDF_SHADER_DIR})
sigil_library(SigilMaterialSdf
  SOURCES Sdf.cpp
  HEADERS Sdf.h
  PUBLIC SigilMaterialCore SigilMaterialColor)
sigil_shader_sources(SigilMaterialSdf
  DIR ${_shaders} NAMESPACE sigil::material::sdf)

# test/SdfTest.cpp is compiled into material_test, at the library root:
# the primitives, the colour leaf and the view transforms all shade
# through the Skia backend, so a binary per feature would be five
# binaries over one link closure.

sigil_bench(material_sdf_bench
  SOURCES bench/SdfBench.cpp
  LIBRARIES SigilMaterialSdf SigilMaterialSkia)
