# Encode: frames into a container through FFmpeg.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMediaVideoEncode
  SOURCES Encode.cpp
  HEADERS ../Encoder.h
  PUBLIC SigilMediaCore
  PRIVATE sigil_media_ffmpeg)

sigil_test(media_test
  SOURCES test/EncodeTest.cpp test/ElementaryStreamTest.cpp
  LIBRARIES SigilMediaVideoEncode SigilMediaVideoDecode)
