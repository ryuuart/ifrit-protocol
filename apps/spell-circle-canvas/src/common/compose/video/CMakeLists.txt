# SigilComposeVideo — a live decoded frame as a retained leaf. The adapter is
# header-only; the decoder owns packets, frames, and device wrapping.
sigil_library(SigilComposeVideo
  HEADERS Video.h
  INTERFACE SigilComposeCore SigilVideoDecode)

sigil_test(compose_video_test
  SOURCES test/ComposeVideoTest.cpp
  LIBRARIES SigilComposeVideo SigilVideoEncode SigilWeavePorts)
