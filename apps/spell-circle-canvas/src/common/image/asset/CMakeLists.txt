# The asset leaf: the decoded document (frames, durations, playback) and
# the Skia-codec decode of PNG, JPEG, WebP, GIF and AVIF (stills and
# animations) into premultiplied SkImage frames ready for any SkCanvas.
# Skia only.
add_library(SigilImageAsset STATIC
  ImageAsset.cpp
  ${SIGIL_IMAGE_INCLUDE_DIR}/sigilimage/asset/ImageAsset.h
)
target_compile_features(SigilImageAsset PUBLIC cxx_std_20)
target_include_directories(SigilImageAsset PUBLIC ${SIGIL_IMAGE_INCLUDE_DIR})
target_link_libraries(SigilImageAsset PUBLIC unofficial::skia::skia)

# Links only the library it exercises.
add_executable(image_asset_test test/AssetTest.cpp)
target_link_libraries(image_asset_test PRIVATE SigilImageAsset GTest::gtest_main)
target_compile_definitions(image_asset_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${SIGIL_IMAGE_TEST_ASSET_DIR}"
)
add_test(NAME image_asset_test COMMAND image_asset_test)
