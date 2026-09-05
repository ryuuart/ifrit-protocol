# The Slang backend: the compiler that turns Slang source into SPIR-V and
# reports where every uniform's bytes go, the program handle a renderer
# reaches that off, and the buffer one draw's uniforms are written into.
# It compiles no material on its own — a renderer hands it a scaffold with
# a recipe's body appended and registers the result with the core's
# program cache — so what lives here is the compile and the layout, and
# nothing that knows a pass, a target or a device.
#
# Portable.slang is the subset whose transcendentals a host and a device
# answer alike. It is compiled by slangc when this library is built, which
# makes a mistake in it a build failure. At run time SigilIO loads that same
# authored file into every session by name; the generated header remains the
# build's validation artifact. Compilers of importing shaders search this
# directory.
#
# The kit is linked for one thing: its shading terms are loaded into every
# session by name, so a renderer's shading and every material body
# compiled beside it call one definition of a term. Slang itself is
# PRIVATE — the public header names none of its types.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(SIGIL_MATERIAL_SLANG_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material Slang backend's shader modules")
sigil_slang_module(NAME Portable
  SOURCE ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
  OUT_VAR SIGIL_MATERIAL_PORTABLE_H)

sigil_library(SigilMaterialSlang
  SOURCES SlangCompiler.cpp ${SIGIL_MATERIAL_PORTABLE_H}
          ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
  HEADERS SlangCompiler.h
  INCLUDE_PRIVATE ${SIGIL_SLANG_GENERATED_DIR}
  PUBLIC SigilMaterialCore Boost::container
  PRIVATE SigilMaterialKit SigilIOHub slang::slang)
target_compile_definitions(SigilMaterialSlang
  PRIVATE SIGIL_MATERIAL_SLANG_SHADER_DIR="${SIGIL_MATERIAL_SLANG_SHADER_DIR}")

sigil_test(material_slang_test
  SOURCES test/SlangTest.cpp
  LIBRARIES SigilMaterialSlang SigilMaterialKit)

sigil_bench(material_slang_bench
  SOURCES bench/SlangBench.cpp
  LIBRARIES SigilMaterialSlang)
