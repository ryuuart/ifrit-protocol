# SigilSubstance — .sbsar procedural materials rendered to SkImages
# through the Adobe Substance 3D SDK's framework and CPU engine. Only
# configured when the SDK was found; nothing here is vendored from the
# SDK.
#
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

# workaround: the Substance engine dylib's install name is @rpath-relative.
#
# The framework is a static archive over one engine dylib; the dylib's
# install name is @rpath-relative, so every executable linking this
# library carries the SDK's bin/release in its runtime search path.
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
get_filename_component(_substance_bin "${SUBSTANCE_SDK_DIR}/bin/release" ABSOLUTE)
set_property(TARGET SigilSubstance PROPERTY INTERFACE_LINK_OPTIONS
  "LINKER:-rpath,${_substance_bin}")

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
