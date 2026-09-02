# SigilImage — Qt-free image import for the scene canvases, as one
# feature library per directory. Each feature is its own static archive
# with its own tests and benchmarks, and links only the features beneath
# it, so a consumer that needs one tier links that tier alone.
# `SigilImage` is the umbrella: an interface over every feature, for a
# consumer of the whole library.
#
# Every public header lives under one include root, include/sigilimage/
# <feature>/, so a feature adds the root and spells its own directory.
# The committed fixtures every test and bench reads sit in test/assets/
# (4x4 px, a few hundred bytes each — one still per format plus a
# 3-frame red/green/blue animation for each animated format, regenerable
# with PIL + avifenc) and reach a binary through the
# IFRIT_IMAGE_TEST_ASSET_DIR compile definition.
#
# The Skia codec set is guaranteed by the skia vcpkg features declared in
# vcpkg.json (png/jpeg/webp are port defaults; avif is opted in there).
# The port's avif feature carries libavif[dav1d], because a libavif built
# with no AV1 codec parses an AVIF container and decodes no frame out of
# it.

set(SIGIL_IMAGE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SIGIL_IMAGE_TEST_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/test/assets)
# The helpers every image test reads a fixture by. A test target adds
# this directory and spells "Pixels.h".
set(SIGIL_IMAGE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test)

# Dependencies point down this list.
add_subdirectory(asset)
add_subdirectory(decode)
add_subdirectory(encode)

add_library(SigilImage INTERFACE)
target_link_libraries(SigilImage INTERFACE SigilImageAsset SigilImageDecode
  SigilImageEncode)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilImage
  BRIEF "Image decoding and encoding: bytes in, Skia images out, and back"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
