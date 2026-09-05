# SigilSkia — Skia Graphite brought up on a GPU device someone else owns,
# as one feature library per directory. The graphite feature stands a
# Graphite Context and Recorder up on a native device and queue, wraps a
# native texture as an SkSurface, and reads an image out at the width a
# device texture takes it — Metal and Vulkan as parallel paths, with no
# Qt in it. The qt feature is the adapter a Qt host uses: it unwraps a
# QRhi's native handles and forwards to the graphite feature. `SigilSkia`
# is the umbrella over every Qt-free feature; a Qt host links
# `SigilSkiaQt`, which carries the umbrella with it.
#
# THE DEVICE IS NOT HERE. It is SigilCoreHardware's — the device value,
# its handles and its fences know nothing of Skia, so a Diligent renderer
# and Graphite stand on the same one. Both features here link it.
#
# Every public header lives under one include root, include/sigilskia/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilskia/Skia.h> is the umbrella header over the Qt-free features.
#
# Nothing here knows what is being drawn. The GRAPHITE bring-up and its
# Qt adapter are added only when SPELLCIRCLE_ENABLE_SKIA_CANVAS is on
# (src/CMakeLists.txt); the draw feature is unconditional, because the
# ops it decomposes vanish on a Graphite canvas whether or not this
# repository is the one that stood that canvas up.

set(SIGIL_SKIA_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point up this list.
add_subdirectory(draw)

if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  # The Vulkan headers, for the graphite feature's Vulkan bring-up; it
  # links them privately, since every public header spells a Vulkan handle
  # as an opaque value.
  find_package(VulkanHeaders CONFIG REQUIRED)

  add_subdirectory(graphite)
  add_subdirectory(qt)

  add_library(SigilSkia INTERFACE)
  target_link_libraries(SigilSkia INTERFACE SigilSkiaGraphite SigilSkiaDraw)
endif()

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SigilSkia
  BRIEF "Skia Graphite bring-up on a Metal or Vulkan device someone else owns"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
