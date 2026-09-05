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
# <sigilskia/Skia.h> is the umbrella header over the Qt-free features.
# Nothing here knows what is being drawn.
sigil_library_root(SigilSkia
  BRIEF "Skia Graphite bring-up on a Metal or Vulkan device someone else owns")

# Dependencies point up this list.
add_subdirectory(draw)
add_subdirectory(graphite)
add_subdirectory(qt)

sigil_library(SigilSkia
  INTERFACE SigilSkiaGraphite SigilSkiaDraw)
