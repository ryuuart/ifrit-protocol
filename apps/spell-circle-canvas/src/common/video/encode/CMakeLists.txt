add_library(SigilVideoEncode STATIC
  Encode.cpp
  ${SIGIL_VIDEO_INCLUDE_DIR}/sigilvideo/encode/Encode.h
)
target_compile_features(SigilVideoEncode PUBLIC cxx_std_20)
target_include_directories(SigilVideoEncode PUBLIC ${SIGIL_VIDEO_INCLUDE_DIR})
target_link_libraries(SigilVideoEncode PUBLIC SigilVideoCore
  unofficial::skia::skia)
_sigil_video_ffmpeg(SigilVideoEncode)

add_executable(video_encode_test test/EncodeTest.cpp)
target_link_libraries(video_encode_test PRIVATE SigilVideoEncode
  SigilVideoDecode GTest::gtest_main)
add_test(NAME video_encode_test COMMAND video_encode_test)
