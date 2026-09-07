# SigilMaterial — materials as recipe instances, as one feature library per
# directory. The core feature is the value model: a recipe (a reflected
# params struct plus one body per shading language), the material that
# instantiates it, and the one program cache that compiles through the
# compilers backends register. There are two of those backends: the Skia
# one compiles a recipe's SkSL body into an SkRuntimeEffect, the Slang one
# compiles its Slang body to SPIR-V and reports the layout a device
# renderer writes uniforms at. The texture feature is the image side: a
# texture as a comparable leaf, the tools' texture sets, the environment
# and bevel producers, the atlas. The mask feature is the third operand
# of over(): where on a surface something applies, as a material read as
# a scalar — a primitive of the value model that needs a texture, which
# is why it stands beside the texture feature and not inside the core. Each backend is its own feature that
# links the core and its renderer, so a consumer of the model alone links
# no renderer. The colour feature is the leaf under the core: the value
# a params struct holds and the OKLab round trip, linking nothing. The
# sdf, pattern and field features are the primitives — fully
# parameterised generators — the ocio feature bakes OpenColorIO's view
# transforms to LUT materials over the texture feature, and the kit
# holds the presets over them. The stock feature gathers every catalogue
# into the one list a host warms in one call, which is why it is the one
# target that links the primitives and the kit together.
# `SigilMaterial` is the umbrella: an
# interface over every feature, for a consumer of the whole library.
# <sigilmaterial/Material.h> is the umbrella header over the core.
sigil_library_root(SigilMaterial
  BRIEF "Materials as recipe instances: reflected params, per-target bodies, one program cache"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/COLOUR.md
       ${CMAKE_CURRENT_SOURCE_DIR}/PAINT.md)

# Dependencies point up this list.
add_subdirectory(color)
add_subdirectory(core)
add_subdirectory(texture)
add_subdirectory(mask)
add_subdirectory(ocio)
add_subdirectory(sdf)
add_subdirectory(pattern)
add_subdirectory(field)
add_subdirectory(skia)
add_subdirectory(kit)
add_subdirectory(stock)
add_subdirectory(slang)

sigil_library(SigilMaterial
  INTERFACE SigilMaterialColor SigilMaterialCore SigilMaterialTexture
            SigilMaterialMask SigilMaterialOcio SigilMaterialSdf
            SigilMaterialPattern SigilMaterialField SigilMaterialSkia
            SigilMaterialKit SigilMaterialStock SigilMaterialSlang)

# THE PRIMITIVES AND THE LEAF, in one binary. The colour value, the
# signed-distance surfaces, the tiles, the fields and the baked view
# transforms are five features, and a test of any of them shades through
# the Skia backend — so five binaries would be five names over one link
# closure and no boundary between them anybody could read. The features
# themselves stay five archives: a consumer of the colour leaf still
# links the colour leaf alone.
#
# The `ocio` label says the view-transform cases need OpenColorIO's
# built-in config; every other case here runs everywhere. The shader
# directories are named for the cases that ask whether each embedded
# table holds every file in its directory.
sigil_test(material_test
  SOURCES color/test/ColorTest.cpp color/test/ExtractTest.cpp
          color/test/RampTest.cpp field/test/FieldTest.cpp
          ocio/test/OcioTest.cpp pattern/test/PatternTest.cpp
          pattern/test/WeaveTest.cpp sdf/test/SdfTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SDF_SHADER_DIR="${SIGIL_MATERIAL_SDF_SHADER_DIR}"
              SIGIL_MATERIAL_FIELD_SHADER_DIR="${SIGIL_MATERIAL_FIELD_SHADER_DIR}"
  LIBRARIES SigilMaterialColor SigilMaterialField SigilMaterialOcio
            SigilMaterialPattern SigilMaterialSdf SigilMaterialSkia
  SUITES Ocio LABELS ocio)

# EVERY API NAME THE PROSE SPELLS, compiled against the headers that own
# it — README.md and both chapters, because a chapter is the same canon
# as the page that links it and prose nobody compiles is prose that goes
# stale. The translation unit is GENERATED from the documents on every
# build, so a passage written today is covered today and a rename the
# prose missed is a build break rather than a confident wrong answer; the
# generator FAILS on a documented name no header declares, so the guard
# cannot go quiet. The script is the tree's, under src/test, because two
# libraries already want it.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(MATERIAL_API_DOC_PROBES
    ${CMAKE_CURRENT_BINARY_DIR}/MaterialApiDocProbes.cpp)
file(GLOB_RECURSE MATERIAL_PUBLIC_HEADERS
     CONFIGURE_DEPENDS include/sigilmaterial/*.h)
add_custom_command(
  OUTPUT ${MATERIAL_API_DOC_PROBES}
  COMMAND ${Python3_EXECUTABLE} ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          --md ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          --md ${CMAKE_CURRENT_SOURCE_DIR}/COLOUR.md
          --md ${CMAKE_CURRENT_SOURCE_DIR}/PAINT.md
          --include ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilmaterial
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../io/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../image/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include
          --include ${SIGIL_TEST_SUPPORT_DIR}
          --library sigilmaterial
          --namespace sigil::material
          --suite MaterialDocs
          --prelude "<gtest/gtest.h>"
          --prelude "<sigilcore/cache/Policy.h>"
          --prelude "<sigilio/hub/Hub.h>"
          --prelude "<sigilshaders/MaterialKit.h>"
          --prelude "<sigilskia/graphite/GraphiteContext.h>"
          --prelude "<ScratchDir.h>"
          --floors 60,3,12,0
          --out ${MATERIAL_API_DOC_PROBES}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/COLOUR.md
          ${CMAKE_CURRENT_SOURCE_DIR}/PAINT.md
          ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          ${MATERIAL_PUBLIC_HEADERS}
  COMMENT "Extracting the material docs' names into compile probes"
  VERBATIM)

# The generator's own fixtures: a real name must yield a probe, an unreal
# one must fail the run, and an operator spelling must be exempted and
# reported by name. One Python invocation, and the only check that
# notices the EXTRACTOR narrowing, since a generator that silently probes
# less still emits a TU that compiles green.
add_test(NAME material_api_doc_probes_self_test
  COMMAND ${Python3_EXECUTABLE} ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          --self-test)

# The probes are their own object library, because a generated source is
# compiled by a target in the directory that generates it, and this
# library's one test binary is assembled in core/.
# It links every library the prose names — the resource hub the usage
# example opens shaders through, the cache policy and the Graphite
# context the caching and device sections name, and the scratch folder
# the testing section names — because a document that spells a name is
# claiming that name exists wherever it lives.
add_library(material_api_doc_probes OBJECT ${MATERIAL_API_DOC_PROBES})
target_link_libraries(material_api_doc_probes PRIVATE
  SigilMaterial SigilIOHub SigilCoreCache SigilSkiaGraphite GTest::gtest)
target_include_directories(material_api_doc_probes PRIVATE
  ${SIGIL_TEST_SUPPORT_DIR} ${SIGIL_SHADER_GENERATED_DIR})

sigil_test(material_test
  LIBRARIES material_api_doc_probes SigilMaterial)

# THE DEVICE SWEEP, over the whole library rather than one feature: a body
# is validated one way as its own runtime effect and another once a GPU
# backend has inlined it into a pipeline, and every per-feature suite
# compiles on a raster surface, so none of them can see the second. It
# links the umbrella and Graphite, which is why it stands here and not
# under a feature.
if(APPLE)
  sigil_test(material_test
    SOURCES test/MaterialGpuTest.mm
    LIBRARIES SigilMaterial SigilSkia
    SUITES MaterialGpu LABELS gpu)
endif()
