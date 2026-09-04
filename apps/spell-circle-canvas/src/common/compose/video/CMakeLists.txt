# SigilComposeVideo — a live decoded frame as a retained leaf. The adapter is
# header-only; the decoder owns packets, frames, and device wrapping.
add_library(SigilComposeVideo INTERFACE
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/video/Video.h
)
target_link_libraries(SigilComposeVideo INTERFACE SigilComposeCore
  SigilVideoDecode)

add_executable(compose_video_test test/ComposeVideoTest.cpp)
target_link_libraries(compose_video_test PRIVATE SigilComposeVideo
  SigilVideoEncode SigilWeavePorts GTest::gtest_main)
add_test(NAME compose_video_test COMMAND compose_video_test)
