# The decode surface: format-routing decodeImage()/probeImage()/
# decodeChannels() over pluggable backends — the Skia codecs, OpenImageIO
# and the Skia SVG module — and the raw channel planes. The backends are
# PRIVATE to it; a consumer sees bytes in and images out.
add_library(SigilImageDecode STATIC
  ChannelData.cpp
  Decode.cpp
  Oiio.cpp
  SkiaCodec.cpp
  Svg.cpp
  Backends.h
  ${SIGIL_IMAGE_INCLUDE_DIR}/sigilimage/decode/ChannelData.h
  ${SIGIL_IMAGE_INCLUDE_DIR}/sigilimage/decode/Decode.h
)
target_compile_features(SigilImageDecode PUBLIC cxx_std_20)
target_include_directories(SigilImageDecode PUBLIC ${SIGIL_IMAGE_INCLUDE_DIR})
target_link_libraries(SigilImageDecode PUBLIC SigilImageAsset)

# OpenImageIO extends decode/probe beyond the Skia codecs: EXR (with
# layer/channel selection), PSD (composited), TIFF, HDR, and the rest
# of its roster. Optional: without it, decodeImage falls back to the
# Skia path only and Oiio.cpp compiles to nothing.
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
# raster size). Optional: without it, SVG bytes simply fail to decode
# and Svg.cpp compiles to nothing.
if(TARGET unofficial::skia::modules::svg)
  target_link_libraries(SigilImageDecode PRIVATE unofficial::skia::modules::svg)
  target_compile_definitions(SigilImageDecode PRIVATE SIGILIMAGE_HAS_SVG)
  message(STATUS "SigilImageDecode: Skia SVG backend enabled")
else()
  message(WARNING
    "SigilImageDecode: skia svg module not found — SVG decoding disabled")
endif()

# Links only the library it exercises.
add_executable(image_decode_test test/DecodeTest.cpp)
target_link_libraries(image_decode_test PRIVATE SigilImageDecode GTest::gtest_main)
if(TARGET unofficial::skia::modules::svg)
  target_compile_definitions(image_decode_test PRIVATE SIGILIMAGE_HAS_SVG)
endif()
target_compile_definitions(image_decode_test PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${SIGIL_IMAGE_TEST_ASSET_DIR}"
)
add_test(NAME image_decode_test COMMAND image_decode_test)

# The decode benchmark (Google Benchmark; use a Release build) encodes
# its own fixtures in memory through the encode feature and reads the
# committed 4 x 4 ones for the per-call floor. Not a test: it builds
# through the `benches` target and runs through scripts/bench_ledger.py.
add_executable(image_decode_bench bench/DecodeBench.cpp)
target_link_libraries(image_decode_bench PRIVATE SigilImageDecode
  SigilImageEncode benchmark::benchmark)
target_compile_definitions(image_decode_bench PRIVATE
  IFRIT_IMAGE_TEST_ASSET_DIR="${SIGIL_IMAGE_TEST_ASSET_DIR}"
)
add_dependencies(benches image_decode_bench)
