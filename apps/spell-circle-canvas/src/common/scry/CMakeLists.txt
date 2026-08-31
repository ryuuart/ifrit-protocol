# SigilScry — HTML/CSS/JS layout for the scene canvases, rendered by the
# Ultralight SDK (WebKit-derived) into SkImage frames, as one feature
# library per directory. The platform feature is what Ultralight's
# Platform singleton is handed: an SkBitmap-backed surface, the two-root
# file system, the logger bridge and the resource directory. The gpu
# feature executes Ultralight's GPU command lists on a SigilSkia
# GpuDevice and gives the engine its texture interop — Metal today, the
# seam a Vulkan driver implements. The engine feature is the web thread,
# the views that publish frames and the image slots pages display.
# `SigilScry` is the umbrella over all three.
#
# Every public header lives under one include root, include/sigilscry/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilscry/Scry.h> is the umbrella header. The platform and gpu
# features also carry headers that name Ultralight types; Ultralight is
# private to the library, so those stay beside their sources and the
# engine feature reaches them by directory rather than the include root.
#
# Only added when SPELLCIRCLE_ENABLE_ULTRALIGHT is ON (auto-off when the
# SDK is missing; see cmake/FindUltralight.cmake). Textures are named on
# a SigilSkia GpuDevice and the GPU driver draws over a shared SigilSkia
# Graphite context, so the library needs SigilSkia
# (SPELLCIRCLE_ENABLE_SKIA_CANVAS).
if(NOT TARGET SigilSkia)
  message(STATUS "SigilScry skipped: needs SPELLCIRCLE_ENABLE_SKIA_CANVAS (SigilSkia)")
  return()
endif()

set(SIGIL_SCRY_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
endif()

# Dependencies point up this list.
add_subdirectory(platform)
add_subdirectory(gpu)
add_subdirectory(engine)

add_library(SigilScry INTERFACE)
target_link_libraries(SigilScry INTERFACE
  SigilScryPlatform SigilScryGpu SigilScryEngine)

# Headless demo: renders HTML layouts through SigilScry and composites them
# with direct SkCanvas drawing into PNGs (out dir: first argument,
# default ./scry_demo_out).
add_executable(scry_demo examples/scry_demo.cpp)
target_link_libraries(scry_demo PRIVATE SigilScry)
ultralight_copy_resources(scry_demo)

# The all-hardware demo: Ultralight's Metal driver feeding texture-backed
# SkImages into a Graphite surface on the same device and queue.
if(APPLE)
  add_executable(scry_gpu_demo examples/scry_gpu_demo.mm)
  target_link_libraries(scry_gpu_demo PRIVATE SigilScry ${METAL_FRAMEWORK})
  ultralight_copy_resources(scry_gpu_demo)
endif()

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilScry
  BRIEF "A headless web engine whose HTML, CSS and JavaScript output arrives as Skia images"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
