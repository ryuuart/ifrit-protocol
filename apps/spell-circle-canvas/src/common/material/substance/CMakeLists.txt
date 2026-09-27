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
else()
  target_sources(SigilMaterialSubstance PRIVATE Unavailable.cpp)
  message(STATUS
    "Substance SDK not found — <sigilmaterial/substance/Substance.h> "
    "answers unavailable")
endif()

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
sigil_test(material_test
  SOURCES test/SubstanceTest.cpp
  LIBRARIES SigilMaterialSubstance SigilIOHub
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
sigil_test(material_test SUITES Substance LABELS substance)

sigil_bench(material_bench
  SOURCES bench/SubstanceBench.cpp
  LIBRARIES SigilMaterialSubstance SigilIOHub
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
