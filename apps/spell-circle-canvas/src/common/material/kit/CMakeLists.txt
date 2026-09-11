# The kit: presets — the reflective surfaces over a normal map and an
# environment map, the grained surfaces (stone, timber, latten, board),
# the orthographic globe, the girih panel, the gel and chrome colour
# tables, the text paints, and the two named skies a reflective surface
# is lit by.
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

find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMaterialKit
  SOURCES Environments.cpp Globe.cpp Grained.cpp LayerStyles.cpp
          Patterns.cpp Ramps.cpp Recipes.cpp Pbr.cpp Reflections.cpp
          TextPaint.cpp
  HEADERS Environments.h Globe.h Grained.h LayerStyles.h Patterns.h Ramps.h
          Recipes.h Pbr.h Reflections.h TextPaint.h
  PUBLIC SigilMaterialPattern SigilMaterialColor SigilMaterialMask
         Boost::container)
sigil_shader_sources(SigilMaterialKit
  DIR ${SIGIL_MATERIAL_KIT_SHADER_DIR} NAMESPACE sigil::material::kit)

# The test draws, so it links the Skia backend beside the kit. The
# shader directory is named for the case that asks whether the embedded
# table holds every file in it.
# The terms suite links SigilGeometryMeshRender for the one case that
# asks whether the C++ a host tier shades with answers what this
# library's shader text answers. It is the only edge from a test here to
# another library, and it is the point of the case: two spellings of one
# arithmetic cannot be compared from inside one of them.
sigil_test(material_test
  SOURCES test/KitTest.cpp test/TermsTest.cpp test/GrainedTest.cpp
          test/GlobeTest.cpp test/RampsTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_KIT_SHADER_DIR="${SIGIL_MATERIAL_KIT_SHADER_DIR}"
  LIBRARIES SigilMaterialKit SigilMaterialSkia SigilGeometryMeshRender)

sigil_bench(material_bench
  SOURCES bench/KitBench.cpp
  LIBRARIES SigilMaterialKit SigilMaterialSkia)
