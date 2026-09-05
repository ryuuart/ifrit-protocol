# Encode: frames into a container through FFmpeg.
sigil_library(SigilVideoEncode
  SOURCES Encode.cpp
  HEADERS Encode.h
  PUBLIC SigilVideoCore unofficial::skia::skia
  PRIVATE PkgConfig::FFMPEG)

sigil_test(video_encode_test
  SOURCES test/EncodeTest.cpp
  LIBRARIES SigilVideoEncode SigilVideoDecode)
