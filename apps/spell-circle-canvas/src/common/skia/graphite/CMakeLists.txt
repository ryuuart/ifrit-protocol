# The graphite feature: a Graphite Context and Recorder on a native device
# and queue, an SkSurface over a native texture, and the pixel reads a
# device upload takes. One API-independent translation unit per class
# holds the options and the submit; the Metal and Vulkan translation units
# are the parallel bring-up paths, each defining its own factory and
# wrapping constructor. The Vulkan units compile everywhere and are live
# only where the linked Skia carries the backend; the Metal units exist on
# Apple alone.
#
# The `OnDevice` units are the entry points that name a texture or a fence
# by hardware handle: Graphite is what stands on a device, so the bring-up
# over one lives here and SigilCoreHardware below stays free of Skia.
#
# Links Skia and SigilCoreHardware, the Vulkan headers privately (the
# public headers spell every Vulkan handle as an opaque value, so no
# consumer needs them) and, on Apple, Foundation — no Qt.
find_package(VulkanHeaders CONFIG REQUIRED)
if(APPLE)
  set(SIGIL_SKIA_GRAPHITE_METAL_SOURCES
      GraphiteContextMetal.mm
      OffscreenSurfaceMetal.mm)
else()
  set(SIGIL_SKIA_GRAPHITE_METAL_SOURCES)
endif()

sigil_library(SigilSkiaGraphite
  SOURCES GraphiteContext.cpp GraphiteContextVulkan.cpp GraphiteOnDevice.cpp
          OffscreenSurface.cpp OffscreenSurfaceOnDevice.cpp
          OffscreenSurfaceVulkan.cpp Pixels.cpp
          ${SIGIL_SKIA_GRAPHITE_METAL_SOURCES}
  HEADERS ../Skia.h GraphiteContext.h OffscreenSurface.h Pixels.h
  PUBLIC unofficial::skia::skia SigilCoreHardware
  PRIVATE Vulkan::Headers Boost::unordered)
if(APPLE)
  target_link_libraries(SigilSkiaGraphite PRIVATE ${FOUNDATION_FRAMEWORK})
endif()

# The Graphite readback every binary that draws on a device reads: this
# target owns the context, so the one spelling of the snap-insert-read-
# submit sequence lives beside it. Named so a consumer in another library
# can put it on its include path.
set(SIGIL_SKIA_GRAPHITE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test/support
    CACHE INTERNAL "the shared Graphite readback")

# THE PIXEL READS ARE ARITHMETIC over an SkImage — no device, no context,
# no bring-up — so they are their own binary, built and run everywhere,
# where the device cases below are Apple-only and labelled.
sigil_test(skia_pixels_test
  SOURCES test/PixelsTest.cpp
  LIBRARIES SigilSkiaGraphite)

# The Metal path end to end on the system device, and the same wrap
# reached through a device's handles: needs a GPU, so both exist only
# where one is guaranteed.
if(APPLE)
  sigil_test(skia_graphite_test
    SOURCES test/GraphiteTest.mm
    LIBRARIES SigilSkiaGraphite ${METAL_FRAMEWORK}
    LABELS gpu
    ARC)

  sigil_bench(skia_graphite_bench
    SOURCES bench/GraphiteBench.mm
    LIBRARIES SigilSkiaGraphite ${METAL_FRAMEWORK}
    ARC)
endif()
