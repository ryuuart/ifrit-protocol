# SigilSubstance — .sbsar procedural materials rendered to SkImages
# through the Adobe Substance 3D SDK's framework and CPU engine. The SDK
# is a licensed download that is never vendored and has no vcpkg port, so
# the find module beside this file is what locates an installed copy.
# Without one the whole library is left out of the build.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
find_package(Substance)
if(NOT Substance_FOUND)
  message(STATUS "Substance SDK not found -- SigilSubstance is left out")
  return()
endif()
# The CPU engine ("blend": results in system memory), matched to the host:
# NEON on Apple Silicon, SSE2 on x86-64. Headless, no GPU context to own,
# and its outputs land as bytes a consumer uploads like any other image.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT neon_blend)
else()
  set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT sse2_blend)
endif()
include("${SUBSTANCE_SDK_DIR}/substance-config.cmake")

# One static target over two subjects: graph/ (a graph described,
# changed, cooked and read) and package/ (the archive that owns its
# graphs and the engine renderer they share). They are one feature
# because neither exists without the other — a Graph is only ever
# constructed by its Package, and a Package is nothing but its graphs —
# so a test of one is a test of both. <sigilsubstance/Substance.h> is
# the umbrella over them. The package subject fills a Graph through
# graph/GraphImpl.h, an internal header spelled by its subject directory,
# which is why this directory is on the private include path.
sigil_library_root(SigilSubstance
  BRIEF "Adobe Substance 3D archives rendered to channel-tagged texture sets")

# The framework is a static archive over one engine dylib whose install
# name is @rpath-relative; the dylib is linked by its full path, which is
# what puts the SDK's bin/release in every linking executable's runtime
# search path.
find_package(boost_container CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilSubstance
  SOURCES graph/Describe.cpp graph/Graph.cpp graph/GraphImpl.h
          graph/Inputs.cpp graph/Numeric.h graph/Render.cpp
          package/Package.cpp
  HEADERS Substance.h graph/Graph.h graph/Output.h graph/Parameter.h
          package/Package.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC unofficial::skia::skia Boost::container
  PRIVATE Substance::Framework)

# The test and the benchmark render the SDK's own sample archives, found
# through this definition; both skip, rather than fail, when the samples
# are not where the SDK was configured from.
sigil_test(substance_test
  SOURCES test/SubstanceTest.cpp
  LIBRARIES SigilSubstance
  LABELS substance
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")

sigil_bench(substance_bench
  SOURCES bench/SubstanceBench.cpp
  LIBRARIES SigilSubstance
  DEFINITIONS SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
