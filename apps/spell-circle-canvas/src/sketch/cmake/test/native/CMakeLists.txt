set(SIGIL_NATIVE_TEST_BOUNDARY_DIR "${CMAKE_CURRENT_LIST_DIR}/boundary" CACHE PATH
    "Mutable originating-library inputs for the native boundary integration test")
set(SIGIL_NATIVE_TEST_PLUGIN_SOURCE "${CMAKE_CURRENT_LIST_DIR}/Plugin.cpp" CACHE FILEPATH
    "Native boundary integration plugin source")

add_library(SigilSketchNativeFixture INTERFACE
  "${SIGIL_NATIVE_TEST_BOUNDARY_DIR}/include/Boundary.h")
target_include_directories(SigilSketchNativeFixture INTERFACE
  "${SIGIL_NATIVE_TEST_BOUNDARY_DIR}/include")
# This library is configured beside the consumed origin and never consumed.
add_library(SigilSketchUnconsumedFixture INTERFACE
  "${SIGIL_NATIVE_TEST_BOUNDARY_DIR}/include/Unrelated.h")
target_include_directories(SigilSketchUnconsumedFixture INTERFACE
  "${SIGIL_NATIVE_TEST_BOUNDARY_DIR}/include")
target_link_libraries(SigilSketchNativeFixture INTERFACE
  "$<$<BOOL:0>:SigilSketchUnconsumedFixture>")
# A target declared under a test directory is no origin unless it says so.
# The case reads both fixtures' identities from the catalog.
set_property(GLOBAL APPEND PROPERTY SIGIL_SKETCH_PLUGIN_ORIGINS
  SigilSketchNativeFixture SigilSketchUnconsumedFixture)
include("${SIGIL_NATIVE_TEST_BOUNDARY_DIR}/Build.cmake")

sigil_sketch_plugin(sketch_native_plugin
  SOURCES "${SIGIL_NATIVE_TEST_PLUGIN_SOURCE}"
  LIBRARIES SigilComposeCore SigilSketchNativeFixture)
add_executable(sketch_native_host Host.cpp)
target_link_libraries(sketch_native_host PRIVATE SigilSketch SigilWeavePorts)
include("${CMAKE_CURRENT_LIST_DIR}/../../SketchLinkSurface.cmake")
if(APPLE)
  target_link_options(sketch_native_host PRIVATE "LINKER:-export_dynamic")
  sigil_sketch_link_surface(sketch_native_host SigilSketch
    EXTRA unofficial::skia::skia)
else()
  target_link_options(sketch_native_host PRIVATE "LINKER:--export-dynamic")
endif()
add_executable(sketch_compose_consumer ComposeConsumer.cpp)
target_link_libraries(sketch_compose_consumer PRIVATE SigilComposeCore SigilWeavePorts)
add_library(sketch_typography_usage OBJECT EXCLUDE_FROM_ALL TypographyConsumer.cpp)
target_link_libraries(sketch_typography_usage PRIVATE "$<COMPILE_ONLY:SigilComposeTypography>")
add_executable(sketch_typography_missing EXCLUDE_FROM_ALL
  $<TARGET_OBJECTS:sketch_typography_usage>)
target_link_libraries(sketch_typography_missing PRIVATE SigilComposeCore SigilWeavePorts)
add_executable(sketch_typography_consumer EXCLUDE_FROM_ALL
  $<TARGET_OBJECTS:sketch_typography_usage>)
target_link_libraries(sketch_typography_consumer PRIVATE
  SigilComposeCore SigilWeavePorts SigilComposeTypography)
