# IfritImage — Qt-free image import for the scene canvases: decodes PNG,
# JPEG, WebP, GIF, and AVIF (stills and animations) through SkCodec into
# premultiplied SkImage frames ready for any SkCanvas. The codec set is
# guaranteed by the skia vcpkg features declared in vcpkg.json
# (png/jpeg/webp are port defaults; avif is opted in there). vcpkg.json
# also requests libavif[dav1d] explicitly: skia[avif] alone installs
# libavif with NO AV1 codec, which parses AVIF containers but silently
# fails to decode any frames.
add_library(IfritImage STATIC
  ImageAsset.cpp
  include/ifritimage/ImageAsset.h
)

target_compile_features(IfritImage PUBLIC cxx_std_20)

target_include_directories(IfritImage PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(IfritImage
  PUBLIC
    unofficial::skia::skia
)

add_executable(image_test
  test/ImageAssetTest.cpp
)

target_link_libraries(image_test PRIVATE IfritImage GTest::gtest_main)

# Tiny committed fixtures (4x4 px, a few hundred bytes each) — one still
# per format plus a 3-frame red/green/blue animation for each animated
# format, regenerable with PIL + avifenc (see the test).
target_compile_definitions(image_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets"
)

add_test(NAME image_test COMMAND image_test)
