# The Slang backend: the compiler that turns Slang source into SPIR-V and
# reports where every uniform's bytes go, the program handle a renderer
# reaches that off, and the buffer one draw's uniforms are written into.
# It compiles no material on its own — a renderer hands it a scaffold with
# a recipe's body appended and registers the result with the core's
# program cache — so what lives here is the compile and the layout, and
# nothing that knows a pass, a target or a device.
#
# Portable.slang is the subset whose transcendentals a host and a device
# answer alike. Its text is compiled into the archive and loaded into
# every session by name, and slangc compiles it when this library is
# built, which makes a mistake in it a build failure. Compilers of
# importing shaders search this directory.
#
# The core's shading terms are loaded into every session by name, so a
# renderer's shading and every material body compiled beside it call one
# definition of a term. Slang itself is PRIVATE — the public header names
# none of its types.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(SIGIL_MATERIAL_SLANG_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the material Slang backend's shader modules")
sigil_slang_module(NAME Portable
  SOURCE ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
  PROOF_VAR SIGIL_MATERIAL_PORTABLE_PROOF)

find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMaterialSlang
  SOURCES SlangCompiler.cpp ${SIGIL_MATERIAL_PORTABLE_PROOF}
  HEADERS SlangCompiler.h
  PUBLIC SigilMaterialCore Boost::container
  PRIVATE slang::slang)
sigil_shader_sources(SigilMaterialSlang
  DIR ${SIGIL_MATERIAL_SLANG_SHADER_DIR} NAMESPACE sigil::material::slang)

# The shader directory is named for the case that asks whether the
# embedded table holds every file in it.
sigil_test(material_slang_test
  SOURCES test/SlangTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SLANG_SHADER_DIR="${SIGIL_MATERIAL_SLANG_SHADER_DIR}"
  LIBRARIES SigilMaterialSlang SigilMaterialKit)

sigil_bench(material_slang_bench
  SOURCES bench/SlangBench.cpp
  LIBRARIES SigilMaterialSlang)
