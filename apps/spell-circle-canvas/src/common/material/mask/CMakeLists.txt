# The mask feature: WHERE on a surface something applies, as a material
# whose output is read as a scalar. A mask is the third operand of the
# core's over() — the combinator cannot be called without one — so it is
# a primitive of the value model rather than a preset, and it stands
# between the core and every catalogue that stacks. It links the texture
# feature because a sampled mask reads a map, and no renderer: the two
# bodies are shader text compiled into the archive.
set(SIGIL_MATERIAL_MASK_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the mask feature's shader modules")

find_package(glm CONFIG REQUIRED)
sigil_library(SigilMaterialMask
  SOURCES Mask.cpp
  HEADERS Mask.h
  PUBLIC SigilMaterialTexture glm::glm)
# The two bodies are reached under sigil::material::mask: the embedded
# table declares its own shaderSource(), and the core's already stands in
# sigil::material, which is where the feature's own vocabulary lives.
sigil_shader_sources(SigilMaterialMask
  DIR ${SIGIL_MATERIAL_MASK_SHADER_DIR} NAMESPACE sigil::material::mask)

# The test draws, so it links the Skia backend beside the feature. The
# shader directory is named for the case that asks whether the embedded
# table holds every file in it.
sigil_test(material_mask_test
  SOURCES test/MaskTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_MASK_SHADER_DIR="${SIGIL_MATERIAL_MASK_SHADER_DIR}"
  LIBRARIES SigilMaterialMask SigilMaterialSkia)

sigil_bench(material_mask_bench
  SOURCES bench/MaskBench.cpp
  LIBRARIES SigilMaterialMask SigilMaterialSkia)
