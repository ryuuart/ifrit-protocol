# The graphite feature: a Graphite Context and Recorder on a native device
# and queue, and an SkSurface over a native texture. One API-independent
# translation unit per class holds the options and the submit; the Metal
# and Vulkan translation units are the parallel bring-up paths, each
# defining its own factory and wrapping constructor. The Vulkan units
# compile everywhere and are live only where the linked Skia carries the
# backend; the Metal units exist on Apple alone. Links Skia, the Vulkan
# headers privately (the public headers spell every Vulkan handle as an
# opaque value, so no consumer needs them) and, on Apple, Foundation — no
# Qt.
if(APPLE)
  set(SIGIL_SKIA_GRAPHITE_METAL_SOURCES
      GraphiteContextMetal.mm
      OffscreenSurfaceMetal.mm)
else()
  set(SIGIL_SKIA_GRAPHITE_METAL_SOURCES)
endif()

add_library(SigilSkiaGraphite STATIC
  GraphiteContext.cpp
  GraphiteContextVulkan.cpp
  OffscreenSurface.cpp
  OffscreenSurfaceVulkan.cpp
  ${SIGIL_SKIA_GRAPHITE_METAL_SOURCES}
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/Skia.h
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/graphite/GraphiteContext.h
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/graphite/OffscreenSurface.h
)
target_compile_features(SigilSkiaGraphite PUBLIC cxx_std_20)
target_include_directories(SigilSkiaGraphite PUBLIC ${SIGIL_SKIA_INCLUDE_DIR})
target_link_libraries(SigilSkiaGraphite
  PUBLIC unofficial::skia::skia
  PRIVATE Vulkan::Headers)
if(APPLE)
  target_link_libraries(SigilSkiaGraphite PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

# The Metal path end to end on the system device: needs a GPU, so it
# exists only where one is guaranteed.
if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  add_executable(sigilskia_graphite_test test/GraphiteTest.mm)
  set_source_files_properties(test/GraphiteTest.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_link_libraries(sigilskia_graphite_test PRIVATE SigilSkiaGraphite
    GTest::gtest_main ${METAL_FRAMEWORK})
  add_test(NAME sigilskia_graphite_test COMMAND sigilskia_graphite_test)
endif()
