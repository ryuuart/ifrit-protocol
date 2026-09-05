# The kit: presets — the reflective surfaces over a normal map and an
# environment map, the grained surfaces (stone, timber, latten, board) and
# the bank that bounds a field's instances of them, the girih panel, the
# gel and chrome colour tables, the text paints, and the shading TERMS
# those surfaces are composed of.
# Bodies are shader text; the kit links the pattern feature for the tile
# the panel bakes into and no renderer. A backend's declaration is
# visible library-wide under the one include root, so "no renderer" is
# the link line: a kit source that called into a backend would fail to
# link here.
#
# Shading.slang is compiled by slangc when this library is built, which makes a
# mistake in a term a build failure. At run time SigilIO loads that same
# authored file into the compiler session; the generated header remains the
# build's validation artifact. A renderer's own shaders import it, so their
# compilers also search this directory.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(SIGIL_MATERIAL_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material kit's shader modules")
file(GLOB _shader_sources CONFIGURE_DEPENDS
  "${SIGIL_MATERIAL_SHADER_DIR}/*.sksl"
  "${SIGIL_MATERIAL_SHADER_DIR}/*.slang")
sigil_slang_module(NAME Shading SOURCE ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  OUT_VAR SIGIL_MATERIAL_SHADING_H)

sigil_library(SigilMaterialKit
  SOURCES LayerStyles.cpp Mask.cpp Grained.cpp Patterns.cpp Recipes.cpp
          ShaderSources.cpp ShaderSources.h Surface.cpp Surfaces.cpp
          Terms.cpp TextPaint.cpp
          ${SIGIL_MATERIAL_SHADING_H} ${_shader_sources}
  HEADERS Bank.h Grained.h LayerStyles.h Mask.h Patterns.h Recipes.h
          Surface.h Surfaces.h Terms.h TextPaint.h
  INCLUDE_PRIVATE ${SIGIL_SLANG_GENERATED_DIR}
  PUBLIC SigilMaterialPattern SigilMaterialColor Boost::container
  PRIVATE SigilIOHub)
target_compile_definitions(SigilMaterialKit
  PRIVATE SIGIL_MATERIAL_KIT_SHADER_DIR="${SIGIL_MATERIAL_SHADER_DIR}")

# The test draws, so it links the Skia backend beside the kit.
sigil_test(material_kit_test
  SOURCES test/KitTest.cpp
  LIBRARIES SigilMaterialKit SigilMaterialSkia)

sigil_bench(material_kit_bench
  SOURCES bench/KitBench.cpp
  LIBRARIES SigilMaterialKit SigilMaterialSkia)
