# SigilVideo — streaming video decode and encode over FFmpeg, with a
# device executor for native decoded frames and a CPU executor for every
# surface that can accept pixels.
sigil_library_root(SigilVideo
  BRIEF "Streaming video decoding, device composition, and video encoding")

find_package(concurrentqueue CONFIG REQUIRED)

# FFmpeg publishes its interface as pkg-config files and nothing else:
# one per library, each naming the codecs, the system frameworks and the
# other FFmpeg libraries it needs. Read as an imported target, that is
# the whole link — the four libraries this one calls, and whatever the
# build of FFmpeg on this machine put behind them.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                  libavcodec libavformat libavutil libswscale)

sigil_library(SigilVideoCore
  HEADERS Types.h)

add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilVideo
  HEADERS Video.h
  INTERFACE SigilVideoDecode SigilVideoEncode)
