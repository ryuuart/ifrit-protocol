# The encode surface: format-routing encodeImage() over pluggable
# encoders — Skia's own for PNG/JPEG/WebP, OpenImageIO's for EXR — and
# the CPU readback an SkImage needs before any of them can see it. The
# backends are PRIVATE to it; a consumer sees pixels in and bytes out.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilImageEncode
  SOURCES Encode.cpp Oiio.cpp SkiaEncoder.cpp Backends.h
  HEADERS Encode.h
  PUBLIC unofficial::skia::skia)

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
# The library-wide test directory is already on the include path, so the
# round trip is judged by the same pixel comparison a decode is.
sigil_test(image_test
  SOURCES test/EncodeTest.cpp
  LIBRARIES SigilImageEncode SigilImageDecode
  DEFINITIONS $<$<BOOL:${OpenImageIO_FOUND}>:SIGILIMAGE_HAS_OIIO_ENCODE>)

# A generated bitmap through each format per megapixel.
sigil_bench(image_bench
  SOURCES bench/EncodeBench.cpp
  LIBRARIES SigilImageEncode)
