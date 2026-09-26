# The image route: bytes sniffed and handed to the Skia codecs (PNG,
# JPEG, WebP, GIF and AVIF, animation included), a KTX header reader,
# OpenImageIO and the Skia SVG module; the float channel planes; and the
# signature scan that recovers whole images from a container with no
# parser here. The backends are PRIVATE: a consumer sees bytes in and a
# document out.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMediaImageDecode
  SOURCES Channels.cpp Decode.cpp Embedded.cpp Ktx.cpp Oiio.cpp SkiaCodec.cpp
          Svg.cpp Backends.h
  HEADERS ../Channels.h ../Decode.h ../../advanced/Embedded.h
  PUBLIC SigilMediaCore)

# OpenImageIO extends the route beyond the Skia codecs: EXR (with layer
# and channel selection), PSD (composited), TIFF, HDR, DDS and the rest
# of its roster. Optional: without it, those formats fail to decode and
# Oiio.cpp compiles to nothing.
find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilMediaImageDecode PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilMediaImageDecode PRIVATE SIGILMEDIA_HAS_OIIO)
  message(STATUS "SigilMediaImageDecode: OpenImageIO backend enabled")
else()
  message(WARNING
    "SigilMediaImageDecode: OpenImageIO not found — EXR/PSD/TIFF decoding disabled")
endif()

# The Skia SVG module rasterizes vector sources through the same route
# (sniffed by content; ImageOptions::width/height pick the raster size).
# Optional: without it, SVG bytes fail to decode and Svg.cpp compiles to
# nothing.
if(TARGET unofficial::skia::modules::svg)
  target_link_libraries(SigilMediaImageDecode PRIVATE unofficial::skia::modules::svg)
  target_compile_definitions(SigilMediaImageDecode PRIVATE SIGILMEDIA_HAS_SVG)
  message(STATUS "SigilMediaImageDecode: Skia SVG backend enabled")
else()
  message(WARNING
    "SigilMediaImageDecode: skia svg module not found — SVG decoding disabled")
endif()

# The optional-backend cases compile whatever this build has and skip
# with the backend they wanted named, so the labels say what a run on a
# box without them is missing.
sigil_test(media_test
  SOURCES test/DecodeTest.cpp test/EmbeddedTest.cpp
  LIBRARIES SigilMediaImageDecode
  DEFINITIONS $<$<TARGET_EXISTS:unofficial::skia::modules::svg>:SIGILMEDIA_HAS_SVG>
              $<$<BOOL:${OpenImageIO_FOUND}>:SIGILMEDIA_HAS_OIIO>)
sigil_test(media_test SUITES SvgDecode LABELS svg)
sigil_test(media_test SUITES OiioDecode LABELS oiio)

# Encodes its own fixtures in memory through the encode feature and reads
# the committed ones for the per-call floor.
sigil_bench(media_bench
  SOURCES bench/DecodeBench.cpp
  LIBRARIES SigilMediaImageDecode SigilMediaImageEncode)
