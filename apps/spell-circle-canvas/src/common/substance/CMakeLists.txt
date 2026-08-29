# SigilSubstance — .sbsar procedural materials rendered to SkImages
# through the Adobe Substance 3D SDK's framework and CPU engine. Only
# configured when the SDK was found (see the top-level CMakeLists.txt);
# nothing here is vendored from the SDK.

add_library(SigilSubstance STATIC
  Substance.cpp
  include/sigilsubstance/Substance.h
)

target_compile_features(SigilSubstance PUBLIC cxx_std_20)
target_include_directories(SigilSubstance PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# The framework is a static archive over one engine dylib; the dylib's
# install name is @rpath-relative, so every executable linking this
# library carries the SDK's bin/release in its runtime search path.
target_link_libraries(SigilSubstance
  PUBLIC unofficial::skia::skia
  PRIVATE Substance::Framework
)
get_filename_component(_substance_bin "${SUBSTANCE_SDK_DIR}/bin/release" ABSOLUTE)
set_property(TARGET SigilSubstance PROPERTY INTERFACE_LINK_OPTIONS
  "LINKER:-rpath,${_substance_bin}")

add_executable(substance_test test/SubstanceTest.cpp)
target_link_libraries(substance_test PRIVATE SigilSubstance GTest::gtest_main)
# The SDK ships sample archives; the test renders one of them.
target_compile_definitions(substance_test PRIVATE
  SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
add_test(NAME substance_test COMMAND substance_test)

# Describe an archive and render its outputs to PNGs:
#   substance_demo <file.sbsar> [outdir] [log2size] [name=value ...]
add_executable(substance_demo examples/substance_demo.cpp)
target_link_libraries(substance_demo PRIVATE SigilSubstance)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilSubstance
  BRIEF "Adobe Substance 3D archives rendered to channel-tagged texture sets"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
