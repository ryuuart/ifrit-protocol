# The decode surface: format-routing decodeImage()/probeImage()/
# decodeChannels() over pluggable backends — the Skia codecs, OpenImageIO
# and the Skia SVG module — and the raw channel planes. The backends are
# PRIVATE to it; a consumer sees bytes in and images out.
sigil_library(SigilImageDecode
  SOURCES ChannelData.cpp Decode.cpp Oiio.cpp SkiaCodec.cpp Svg.cpp
          Backends.h
  HEADERS ChannelData.h Decode.h
  PUBLIC SigilImageAsset)

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

sigil_test(image_decode_test
  SOURCES test/DecodeTest.cpp
  LIBRARIES SigilImageDecode
  DEFINITIONS $<$<TARGET_EXISTS:unofficial::skia::modules::svg>:SIGILIMAGE_HAS_SVG>)

# Encodes its own fixtures in memory through the encode feature and reads
# the committed ones for the per-call floor.
sigil_bench(image_decode_bench
  SOURCES bench/DecodeBench.cpp
  LIBRARIES SigilImageDecode SigilImageEncode)
