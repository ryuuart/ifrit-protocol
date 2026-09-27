# The substance feature: an Adobe Substance 3D archive cooked into a
# Material. The SDK is a licensed download that is never vendored and has
# no vcpkg port, so the find module beside Slang's locates an installed
# copy. Where there is none the feature still builds, with `available()`
# false and every entrance answering an empty material, so a consumer
# links one name on every machine. It stands apart from the umbrella:
# only a consumer that cooks a graph takes the engine onto its link line.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)
find_package(Substance QUIET)
sigil_library(SigilMaterialSubstance
  SOURCES Substance.cpp
  HEADERS Substance.h advanced/Archive.h advanced/Cook.h
  PUBLIC SigilMaterialCore SigilMediaCore
  PRIVATE SigilMaterialTexture SigilIOHub)
if(Substance_FOUND)
  # The CPU engine ("blend": results in host memory), matched to the
  # host: NEON on Apple Silicon, SSE2 on x86-64. The framework is a static
  # archive over that one engine dylib, linked by its full path, which is
  # what puts the SDK's bin/release in every linking executable's runtime
  # search path.
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT neon_blend)
  else()
    set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT sse2_blend)
  endif()
  include("${SUBSTANCE_SDK_DIR}/substance-config.cmake")
  find_package(unofficial-skia CONFIG REQUIRED)
  target_sources(SigilMaterialSubstance PRIVATE
    Archive.cpp Cook.cpp Pixels.cpp Internal.h)
  target_link_libraries(SigilMaterialSubstance PRIVATE Substance::Framework)
  target_compile_definitions(SigilMaterialSubstance PUBLIC
    SIGILMATERIAL_ENABLE_SUBSTANCE)

  # The GPU engine beside it: on macOS the Metal engine whose results stay
  # on the device as Metal textures ("native", where "blend" would read
  # them back into host memory). It is not linked — the framework takes
  # an engine library at run time and a renderer switches to it — so the
  # CPU engine stays the one every renderer starts on, and a machine whose
  # SDK has no Metal engine, or whose device will not start it, cooks on
  # the CPU with nothing else changed.
  set(_substance_metal_engine
      "${SUBSTANCE_SDK_DIR}/bin/release/libsubstance_mtl_native.dylib")
  if(APPLE AND EXISTS "${_substance_metal_engine}")
    target_sources(SigilMaterialSubstance PRIVATE Metal.mm Metal.h)
    set_source_files_properties(Metal.mm PROPERTIES
      COMPILE_OPTIONS "-fobjc-arc")
    target_compile_definitions(SigilMaterialSubstance PRIVATE
      SIGIL_SUBSTANCE_METAL_ENGINE="${_substance_metal_engine}")
    target_link_libraries(SigilMaterialSubstance PRIVATE
      SigilSkiaGraphite "-framework Metal" "-framework Foundation")
  endif()

  # The input struct of an archive, as a header the build writes: the
  # tool reads the archive through this feature, so it exists only where
  # the SDK does.
  add_executable(sigil_substance_header tool/SubstanceHeader.cpp)
  target_link_libraries(sigil_substance_header PRIVATE SigilMaterialSubstance)
else()
  target_sources(SigilMaterialSubstance PRIVATE Unavailable.cpp)
  message(STATUS
    "Substance SDK not found — <sigilmaterial/substance/Substance.h> "
    "answers unavailable")
endif()

# substance_input_header(<out-var> <archive> <directory>)
#   Writes `<directory>/<stem>.substance.h`, the input struct of
#   <archive>'s first graph, whenever the archive or the tool changes, and
#   names that header in <out-var> for a target to list as a source.
#   Without the SDK there is no tool, nothing is written and <out-var> is
#   empty, so a caller's list of generated headers is simply shorter.
function(substance_input_header out archive directory)
  set(${out} "" PARENT_SCOPE)
  if(NOT TARGET sigil_substance_header)
    return()
  endif()
  get_filename_component(stem ${archive} NAME_WLE)
  set(header ${directory}/${stem}.substance.h)
  add_custom_command(
    OUTPUT ${header}
    COMMAND sigil_substance_header ${archive} -o ${header}
    DEPENDS ${archive} sigil_substance_header
    COMMENT "Generating ${stem}.substance.h"
    VERBATIM)
  set(${out} ${header} PARENT_SCOPE)
endfunction()

# The find module's choice among several installs needs no SDK, so it is
# asked on every machine.
add_test(NAME Substance.FindModuleChoosesTheNewestInstall
  COMMAND ${CMAKE_COMMAND}
          -DSUBSTANCE_INSTALL_MODULE=${CMAKE_CURRENT_LIST_DIR}/../cmake/SubstanceInstall.cmake
          -DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/find_substance_test
          -P ${CMAKE_CURRENT_LIST_DIR}/test/FindSubstanceTest.cmake)

# The cases cook the SDK's own sample archives, found through this
# definition; each skips, rather than fails, without the SDK or its
# samples, and the `substance` label lets a run leave them out.
# The SDK's leaf sample also has its input header written, so a case
# states what the generator makes of a real archive.
set(_generated)
if(EXISTS "${SUBSTANCE_SDK_DIR}/assets/Autumn_Leaves.sbsar")
  substance_input_header(_generated
    "${SUBSTANCE_SDK_DIR}/assets/Autumn_Leaves.sbsar"
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
endif()
if(_generated)
  # A custom command's rule belongs to a target of the directory that
  # states it, and the test binary is created in another.
  add_custom_target(SigilMaterialSubstanceSampleHeader DEPENDS ${_generated})
endif()
sigil_test(material_test
  SOURCES test/SubstanceTest.cpp
  LIBRARIES SigilMaterialSubstance SigilIOHub SigilMaterialTexture
            SigilMaterialSkia SigilSkiaGraphite SigilCoreHardware
  SUPPORT_DIRS ${CMAKE_CURRENT_BINARY_DIR}/generated
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
if(_generated)
  add_dependencies(material_test SigilMaterialSubstanceSampleHeader)
endif()
sigil_test(material_test SUITES Substance LABELS substance)

sigil_bench(material_bench
  SOURCES bench/SubstanceBench.cpp
  LIBRARIES SigilMaterialSubstance SigilIOHub
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
