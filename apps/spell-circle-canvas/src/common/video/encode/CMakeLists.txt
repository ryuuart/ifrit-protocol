# Encode: frames into a container through FFmpeg.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilVideoEncode
  SOURCES Encode.cpp
  HEADERS Encode.h
  PUBLIC SigilVideoCore unofficial::skia::skia
  PRIVATE sigil_video_ffmpeg)

sigil_test(video_test
  SOURCES test/EncodeTest.cpp
  LIBRARIES SigilVideoEncode SigilVideoDecode)
