# The Slang backend: the compiler that turns Slang source into SPIR-V and
# reports where every uniform's bytes go, the program handle a renderer
# reaches that off, and the buffer one draw's uniforms are written into.
# It compiles no material on its own — a renderer hands it a scaffold with
# a recipe's body appended and registers the result with the core's
# program cache — so what lives here is the compile and the layout, and
# nothing that knows a pass, a target or a device.
#
# Portable.slang is the subset whose transcendentals a host and a device
# answer alike. It is compiled by slangc when this library is built —
# which is what makes a mistake in it a build failure — and its text is
# generated into a header, because every session loads it by name so a
# shader's `import Portable;` resolves in memory and nothing is looked for
# on disk. The directory holding it is named here for the compilers of the
# shaders that import it to search.
include(Slang)
set(SIGIL_MATERIAL_SLANG_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material Slang backend's shader modules")
sigil_slang_module(NAME Portable
  SOURCE ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
  OUT_VAR SIGIL_MATERIAL_PORTABLE_H)

add_library(SigilMaterialSlang STATIC
  SlangCompiler.cpp
  ${SIGIL_MATERIAL_PORTABLE_H}
  ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/slang/SlangCompiler.h
)
target_compile_features(SigilMaterialSlang PUBLIC cxx_std_20)
target_include_directories(SigilMaterialSlang
  PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR}
  PRIVATE ${SIGIL_SLANG_GENERATED_DIR})
# The kit is linked for one thing: its shading terms are loaded into every
# session by name, so a renderer's shading and every material body
# compiled beside it call one definition of a term. Slang itself is
# PRIVATE — the public header names none of its types.
target_link_libraries(SigilMaterialSlang
  PUBLIC SigilMaterialCore
  PRIVATE SigilMaterialKit slang::slang)

add_executable(material_slang_test test/SlangTest.cpp)
target_link_libraries(material_slang_test PRIVATE SigilMaterialSlang
  SigilMaterialKit GTest::gtest_main)
add_test(NAME material_slang_test COMMAND material_slang_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(material_slang_bench bench/SlangBench.cpp)
target_link_libraries(material_slang_bench PRIVATE SigilMaterialSlang
  benchmark::benchmark)
add_dependencies(benches material_slang_bench)
