# SigilSkia — Skia Graphite brought up on a GPU device someone else owns,
# as one feature library per directory. The graphite feature stands a
# Graphite Context and Recorder up on a native device and queue and wraps
# a native texture as an SkSurface, Metal and Vulkan as parallel paths,
# with no Qt in it. The device feature is the device itself: owned or
# adopted, its textures and fences named by generation-checked handles,
# destruction deferred past the frames in flight. The qt feature is the
# adapter a Qt host uses: it unwraps a QRhi's native handles and forwards
# to the graphite feature. `SigilSkia` is the umbrella over every Qt-free
# feature; a Qt host links `SigilSkiaQt`, which carries the umbrella with
# it.
#
# Every public header lives under one include root, include/sigilskia/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilskia/Skia.h> is the umbrella header over the Qt-free features.
#
# Nothing here knows what is being drawn. Added only when
# SPELLCIRCLE_ENABLE_SKIA_CANVAS is on (src/CMakeLists.txt).

set(SIGIL_SKIA_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point up this list.
add_subdirectory(graphite)
add_subdirectory(device)
add_subdirectory(qt)

add_library(SigilSkia INTERFACE)
target_link_libraries(SigilSkia INTERFACE SigilSkiaGraphite SigilSkiaDevice)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilSkia
  BRIEF "Skia Graphite bring-up on a Metal or Vulkan device someone else owns"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
