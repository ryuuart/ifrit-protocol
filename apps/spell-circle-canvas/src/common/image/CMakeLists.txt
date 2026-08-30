# SigilImage — Qt-free image import for the scene canvases, as two
# libraries a consumer links by what it uses:
#
#   SigilImageAsset  — <sigilimage/ImageAsset.h>: the decoded document
#                      (frames, durations, playback) and the Skia-codec
#                      decode of PNG, JPEG, WebP, GIF and AVIF (stills
#                      and animations) into premultiplied SkImage frames
#                      ready for any SkCanvas. Skia only.
#   SigilImageDecode — <sigilimage/Decode.h>: the format-routing decode
#                      surface over pluggable backends — the Skia codecs,
#                      OpenImageIO and the Skia SVG module — and the raw
#                      channel planes. The backends are PRIVATE to it.
#
# SigilImage is the umbrella over both. The Skia codec set is guaranteed
# by the skia vcpkg features declared in vcpkg.json (png/jpeg/webp are
# port defaults; avif is opted in there). vcpkg.json also requests
# libavif[dav1d] explicitly: skia[avif] alone installs libavif with NO
# AV1 codec, which parses AVIF containers but silently fails to decode
# any frames.
add_library(SigilImageAsset STATIC
  ImageAsset.cpp
  include/sigilimage/ImageAsset.h
)
target_compile_features(SigilImageAsset PUBLIC cxx_std_20)
target_include_directories(SigilImageAsset PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(SigilImageAsset PUBLIC unofficial::skia::skia)

add_library(SigilImageDecode STATIC
  Decode.cpp
  include/sigilimage/Decode.h
)
target_compile_features(SigilImageDecode PUBLIC cxx_std_20)
target_include_directories(SigilImageDecode PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(SigilImageDecode PUBLIC SigilImageAsset)

# OpenImageIO extends decode/probe beyond the Skia codecs: EXR (with
# layer/channel selection), PSD (composited), TIFF, HDR, and the rest
# of its roster. Optional: without it, decodeImage falls back to the
# Skia path only.
find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilImageDecode PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilImageDecode PRIVATE SIGILIMAGE_HAS_OIIO)
  message(STATUS "SigilImageDecode: OpenImageIO backend enabled")
else()
  message(WARNING
    "SigilImageDecode: OpenImageIO not found — EXR/PSD/TIFF decoding disabled")
endif()

# The Skia SVG module rasterizes vector sources through the same decode
# surface (sniffed by content; DecodeOptions::width/height pick the
# raster size). Optional: without it, SVG bytes simply fail to decode.
if(TARGET unofficial::skia::modules::svg)
  target_link_libraries(SigilImageDecode PRIVATE unofficial::skia::modules::svg)
  target_compile_definitions(SigilImageDecode PRIVATE SIGILIMAGE_HAS_SVG)
  message(STATUS "SigilImageDecode: Skia SVG backend enabled")
else()
  message(WARNING
    "SigilImageDecode: skia svg module not found — SVG decoding disabled")
endif()

add_library(SigilImage INTERFACE)
target_link_libraries(SigilImage INTERFACE SigilImageAsset SigilImageDecode)

# Tiny committed fixtures (4x4 px, a few hundred bytes each) — one still
# per format plus a 3-frame red/green/blue animation for each animated
# format, regenerable with PIL + avifenc (see the tests). Each test links
# only the library it exercises.
add_executable(image_asset_test test/ImageAssetTest.cpp)
target_link_libraries(image_asset_test PRIVATE SigilImageAsset GTest::gtest_main)
target_compile_definitions(image_asset_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets"
)
add_test(NAME image_asset_test COMMAND image_asset_test)

add_executable(image_decode_test test/ImageDecodeTest.cpp)
target_link_libraries(image_decode_test PRIVATE SigilImageDecode GTest::gtest_main)
if(TARGET unofficial::skia::modules::svg)
  target_compile_definitions(image_decode_test PRIVATE SIGILIMAGE_HAS_SVG)
endif()
target_compile_definitions(image_decode_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets"
)
add_test(NAME image_decode_test COMMAND image_decode_test)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilImage
  BRIEF "Image decoding: encoded bytes in, Skia images out"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
