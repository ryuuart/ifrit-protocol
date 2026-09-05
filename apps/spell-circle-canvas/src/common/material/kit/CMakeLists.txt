# The kit: presets — the reflective surfaces over a normal map and an
# environment map, the grained surfaces (stone, timber, latten, board),
# the girih panel, the gel and chrome colour tables, the text paints, and
# the two named skies a reflective surface is lit by.
# Bodies are shader text; the kit links the pattern feature for the tile
# the panel bakes into and no renderer. A backend's declaration is
# visible library-wide under the one include root, so "no renderer" is
# the link line: a kit source that called into a backend would fail to
# link here. The shading TERMS the surfaces are composed of are the
# core's, so a backend reaches them without reaching a preset.
#
# Every body here is compiled into the archive, so the kit carries its
# presets wherever the binary runs.
set(SIGIL_MATERIAL_KIT_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material kit's shader modules")

sigil_library(SigilMaterialKit
  SOURCES Environments.cpp Grained.cpp LayerStyles.cpp Mask.cpp Patterns.cpp
          Recipes.cpp Surface.cpp Surfaces.cpp TextPaint.cpp
  HEADERS Environments.h Grained.h LayerStyles.h Mask.h Patterns.h Recipes.h
          Surface.h Surfaces.h TextPaint.h
  PUBLIC SigilMaterialPattern SigilMaterialColor Boost::container)
sigil_shader_sources(SigilMaterialKit
  DIR ${SIGIL_MATERIAL_KIT_SHADER_DIR} NAMESPACE sigil::material::kit)

# The test draws, so it links the Skia backend beside the kit. The
# shader directory is named for the case that asks whether the embedded
# table holds every file in it.
sigil_test(material_kit_test
  SOURCES test/KitTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_KIT_SHADER_DIR="${SIGIL_MATERIAL_KIT_SHADER_DIR}"
  LIBRARIES SigilMaterialKit SigilMaterialSkia)

sigil_bench(material_kit_bench
  SOURCES bench/KitBench.cpp
  LIBRARIES SigilMaterialKit SigilMaterialSkia)
