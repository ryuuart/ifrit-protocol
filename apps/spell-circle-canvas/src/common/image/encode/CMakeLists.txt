# The encode surface: format-routing encodeImage() over pluggable
# encoders — Skia's own for PNG/JPEG/WebP, OpenImageIO's for EXR — and
# the CPU readback an SkImage needs before any of them can see it. The
# backends are PRIVATE to it; a consumer sees pixels in and bytes out.
add_library(SigilImageEncode STATIC
  Encode.cpp
  Oiio.cpp
  SkiaEncoder.cpp
  Backends.h
  ${SIGIL_IMAGE_INCLUDE_DIR}/sigilimage/encode/Encode.h
)
target_compile_features(SigilImageEncode PUBLIC cxx_std_20)
target_include_directories(SigilImageEncode PUBLIC ${SIGIL_IMAGE_INCLUDE_DIR})
target_link_libraries(SigilImageEncode PUBLIC unofficial::skia::skia)

# OpenImageIO writes the one format Skia has no encoder for. Optional,
# and it degrades the way the decode side does: without it EXR simply
# fails to encode and Oiio.cpp compiles to nothing.
find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilImageEncode PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilImageEncode PRIVATE SIGILIMAGE_HAS_OIIO_ENCODE)
  message(STATUS "SigilImageEncode: OpenImageIO backend enabled")
else()
  message(WARNING
    "SigilImageEncode: OpenImageIO not found — EXR encoding disabled")
endif()

# Links the decode side too, because the claim a round trip makes is
# that what came back out is what went in.
add_executable(image_encode_test test/EncodeTest.cpp)
target_link_libraries(image_encode_test PRIVATE SigilImageEncode
  SigilImageDecode GTest::gtest_main)
if(OpenImageIO_FOUND)
  target_compile_definitions(image_encode_test PRIVATE SIGILIMAGE_HAS_OIIO_ENCODE)
endif()
add_test(NAME image_encode_test COMMAND image_encode_test)

# The encode benchmark (Google Benchmark; use a Release build): a
# generated bitmap through each format per megapixel. Not a test: it
# builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(image_encode_bench bench/EncodeBench.cpp)
target_link_libraries(image_encode_bench PRIVATE SigilImageEncode
  benchmark::benchmark)
add_dependencies(benches image_encode_bench)
