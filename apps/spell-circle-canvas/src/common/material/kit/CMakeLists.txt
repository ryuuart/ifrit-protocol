# The kit: presets — the reflective surfaces over a normal map and an
# environment map, the grained surfaces (stone, timber, latten, board) and
# the bank that bounds a field's instances of them, the girih panel, the
# gel and chrome colour tables, the text paints, and the shading TERMS
# those surfaces are composed of.
# Bodies are shader text; the kit links the pattern feature for the tile
# the panel bakes into and no renderer.
#
# Shading.slang is compiled by slangc when this library is built, which makes a
# mistake in a term a build failure. At run time SigilIO loads that same
# authored file into the compiler session; the generated header remains the
# build's validation artifact. A renderer's own shaders import it, so their
# compilers also search this directory.
include(Slang)
set(SIGIL_MATERIAL_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material kit's shader modules")
file(GLOB _shader_sources CONFIGURE_DEPENDS
  "${SIGIL_MATERIAL_SHADER_DIR}/*.sksl"
  "${SIGIL_MATERIAL_SHADER_DIR}/*.slang")
sigil_slang_module(NAME Shading SOURCE ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  OUT_VAR SIGIL_MATERIAL_SHADING_H)

add_library(SigilMaterialKit STATIC
  LayerStyles.cpp
  Mask.cpp
  Grained.cpp
  Patterns.cpp
  Recipes.cpp
  ShaderSources.cpp
  ShaderSources.h
  Surface.cpp
  Surfaces.cpp
  Terms.cpp
  TextPaint.cpp
  ${SIGIL_MATERIAL_SHADING_H}
  ${_shader_sources}
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Bank.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Grained.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/LayerStyles.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Mask.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Patterns.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Recipes.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Surface.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Surfaces.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Terms.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/TextPaint.h
)
target_compile_features(SigilMaterialKit PUBLIC cxx_std_20)
target_include_directories(SigilMaterialKit
  PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR}
  PRIVATE ${SIGIL_SLANG_GENERATED_DIR})
target_compile_definitions(SigilMaterialKit
  PRIVATE SIGIL_MATERIAL_KIT_SHADER_DIR="${SIGIL_MATERIAL_SHADER_DIR}")
target_link_libraries(SigilMaterialKit PUBLIC SigilMaterialPattern
  SigilMaterialColor Boost::container)
target_link_libraries(SigilMaterialKit PRIVATE SigilIOHub)

# The negative control for "and no renderer": a target that must FAIL to
# build, and a test that builds it and demands the one message that says the
# boundary held. It is an undefined symbol rather than a missing header
# because every material header is public under one include root — a
# backend's declaration is visible library-wide and only its definition is
# out of the kit's reach.
include(BoundaryProbe)
sigil_boundary_probe(
  TARGET material_kit_boundary_probe
  SOURCES BoundaryProbe.cpp
  LIBRARIES SigilMaterialKit
  EXPECT "sigil::material::skia::install" "installEv")

# The test draws, so it links the Skia backend beside the kit.
add_executable(material_kit_test test/KitTest.cpp)
target_link_libraries(material_kit_test PRIVATE SigilMaterialKit
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_kit_test COMMAND material_kit_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_kit_bench bench/KitBench.cpp)
target_link_libraries(material_kit_bench PRIVATE SigilMaterialKit
  SigilMaterialSkia benchmark::benchmark)
add_dependencies(benches material_kit_bench)
