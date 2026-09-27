# The surface feature: the metallic-roughness program a lit renderer
# shades with — its parameter ABI, one slot per map, the lit and the
# unlit recipe, the dressing of a decoded texture set — and the lowering
# of a material's stated response (`Material::surface`) into that
# program. The bodies are embedded, so the program travels with the
# binary. It links the Skia backend privately for one reading: whether a
# material carries an effects stage, which a surface cannot draw.
set(SIGIL_MATERIAL_SURFACE_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material surface's shader modules")

sigil_library(SigilMaterialSurface
  SOURCES Lower.cpp Surface.cpp
  HEADERS Surface.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilMaterialSkia)
sigil_shader_sources(SigilMaterialSurface
  DIR ${SIGIL_MATERIAL_SURFACE_SHADER_DIR}
  NAMESPACE sigil::material::surface)

# The test draws, so it links the Skia backend beside the feature. The
# terms suite links SigilGeometryMeshRender for the one case that asks
# whether the C++ a host tier shades with answers what this library's
# shader text answers — two spellings of one arithmetic cannot be
# compared from inside one of them. The shader directory is named for
# the case that asks whether the embedded table holds every file in it.
sigil_test(material_test
  SOURCES test/SurfaceTest.cpp test/TermsTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SURFACE_SHADER_DIR="${SIGIL_MATERIAL_SURFACE_SHADER_DIR}"
  LIBRARIES SigilMaterialSurface SigilMaterialSkia SigilGeometryMeshRender)

sigil_bench(material_bench
  SOURCES bench/SurfaceBench.cpp
  LIBRARIES SigilMaterialSurface SigilMaterialSkia)
