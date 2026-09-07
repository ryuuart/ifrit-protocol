# SigilScry — HTML/CSS/JS layout for the scene canvases, rendered by the
# Ultralight SDK (WebKit-derived) into SkImage frames, as one feature
# library per directory. The platform feature is what Ultralight's
# Platform singleton is handed: an SkBitmap-backed surface, the two-root
# file system, the logger bridge and the resource directory. The gpu
# feature executes Ultralight's GPU command lists on a SigilCoreHardware
# GpuDevice and gives the engine its texture interop — Metal, with a Vulkan
# driver slotting in beside it. The engine feature is the web thread,
# the views that publish frames and the image slots pages display.
# `SigilScry` is the umbrella over all three.
#
# <sigilscry/Scry.h> is the umbrella header. The platform and gpu
# features also carry headers that name Ultralight types; Ultralight is
# private to the library, so those stay beside their sources and the
# engine feature reaches them by directory rather than the include root.
#
# Textures are named on SigilCoreHardware's GpuDevice and the GPU driver
# draws over a shared Graphite context.
#
# The SDK is a licensed download that is never vendored, and it has no
# vcpkg port yet, so the find module beside this file is what locates an
# installed copy. Without one the whole library — and every target that
# links it — is left out of the build.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
find_package(Ultralight)
if(NOT Ultralight_FOUND)
  message(STATUS "Ultralight SDK not found -- SigilScry is left out")
  return()
endif()

sigil_library_root(SigilScry
  BRIEF "A headless web engine whose HTML, CSS and JavaScript output arrives as Skia images")

# Dependencies point up this list.
add_subdirectory(platform)
add_subdirectory(gpu)
add_subdirectory(engine)

sigil_library(SigilScry
  INTERFACE SigilScryPlatform SigilScryGpu SigilScryEngine)
