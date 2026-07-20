# SigilImage — Qt-free image import for the scene canvases: decodes PNG,
# JPEG, WebP, GIF, and AVIF (stills and animations) through SkCodec into
# premultiplied SkImage frames ready for any SkCanvas. The codec set is
# guaranteed by the skia vcpkg features declared in vcpkg.json
# (png/jpeg/webp are port defaults; avif is opted in there). vcpkg.json
# also requests libavif[dav1d] explicitly: skia[avif] alone installs
# libavif with NO AV1 codec, which parses AVIF containers but silently
# fails to decode any frames.
add_library(SigilImage STATIC
  ImageAsset.cpp
  Decode.cpp
  include/sigilimage/ImageAsset.h
  include/sigilimage/Decode.h
)

target_compile_features(SigilImage PUBLIC cxx_std_20)

target_include_directories(SigilImage PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilImage
  PUBLIC
    unofficial::skia::skia
)

# OpenImageIO extends decode/probe beyond the Skia codecs: EXR (with
# layer/channel selection), PSD (composited), TIFF, HDR, and the rest
# of its roster. Optional: without it, decodeImage falls back to the
# Skia path only.
find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilImage PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilImage PRIVATE SIGILIMAGE_HAS_OIIO)
  message(STATUS "SigilImage: OpenImageIO backend enabled")
else()
  message(WARNING
    "SigilImage: OpenImageIO not found — EXR/PSD/TIFF decoding disabled")
endif()

add_executable(image_test
  test/ImageAssetTest.cpp
)

target_link_libraries(image_test PRIVATE SigilImage GTest::gtest_main)

# Tiny committed fixtures (4x4 px, a few hundred bytes each) — one still
# per format plus a 3-frame red/green/blue animation for each animated
# format, regenerable with PIL + avifenc (see the test).
target_compile_definitions(image_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets"
)

add_test(NAME image_test COMMAND image_test)
