# The still encoders: Skia's own for PNG/JPEG/WebP, OpenImageIO's for EXR,
# and the CPU readback a picture needs before any of them can see it. The
# backends are PRIVATE: a consumer sees pixels in and bytes out.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMediaImageEncode
  SOURCES Encode.cpp Oiio.cpp SkiaEncoder.cpp Backends.h
  HEADERS ../Encode.h
  PUBLIC SigilMediaCore)

# OpenImageIO writes the one format Skia has no encoder for. Optional,
# and it degrades the way the decode side does: without it EXR fails to
# encode and Oiio.cpp compiles to nothing.
find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilMediaImageEncode PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilMediaImageEncode PRIVATE SIGILMEDIA_HAS_OIIO_ENCODE)
  message(STATUS "SigilMediaImageEncode: OpenImageIO backend enabled")
else()
  message(WARNING
    "SigilMediaImageEncode: OpenImageIO not found — EXR encoding disabled")
endif()

# Links the decode side too, because the claim a round trip makes is that
# what came back out is what went in. No backend define reaches the cases:
# canEncode() is the library's own answer to which formats this build
# writes, and a test told by the build instead would be checking the build
# rather than the library.
sigil_test(media_test
  SOURCES test/EncodeTest.cpp
  LIBRARIES SigilMediaImageEncode SigilMediaImageDecode)

# A generated bitmap through each format per megapixel.
sigil_bench(media_bench
  SOURCES bench/EncodeBench.cpp
  LIBRARIES SigilMediaImageEncode)
