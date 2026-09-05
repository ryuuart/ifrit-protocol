# SigilVideo — streaming video decode and encode over FFmpeg, with a
# device executor for native decoded frames and a CPU executor for every
# surface that can accept pixels.
sigil_library_root(SigilVideo
  BRIEF "Streaming video decoding, device composition, and video encoding")

find_package(concurrentqueue CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

# The ffmpeg port publishes a find module and the include and library
# variables it fills — no package, no target — so the one target the
# features link is declared here from those variables. The module
# resolves the release and debug archives as a pair, which the pkg-config
# files beside them cannot: one .pc describes one build. The variables
# name the release and the debug archive of each library as an
# `optimized;<a>;debug;<b>` pair, which only target_link_libraries reads,
# so the interface is built with the commands and not by setting the
# properties.
#
# workaround: the port's find module fills those variables once and caches
# them; a later configure finds them cached, skips the module, and falls
# through to the port's wrapper, which re-derives openh264 and vpx from
# their .pc files and appends them as `PkgConfig::` targets. A pkg-config
# result is one archive for every configuration, so those two put a debug
# archive on the Release link beside the release one the pair already
# names. They are dropped here; nothing is lost, because each is in the
# pair list under both configurations.
set(_ffmpeg_libraries ${FFMPEG_LIBRARIES})
list(FILTER _ffmpeg_libraries EXCLUDE REGEX "^PkgConfig::")

# workaround: FFMPEG_LIBRARY_DIRS names the release prefix and the debug
# prefix together, and CMake turns a link directory into a runtime search
# path, so handing it whole to target_link_directories() writes an rpath
# into the debug prefix onto every consumer — the app bundle included.
# Every archive above is named by its full path, so no link directory is
# needed at all.
add_library(sigil_video_ffmpeg INTERFACE)
target_include_directories(sigil_video_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(sigil_video_ffmpeg INTERFACE ${_ffmpeg_libraries})
unset(_ffmpeg_libraries)

sigil_library(SigilVideoCore
  HEADERS Types.h)

add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilVideo
  HEADERS Video.h
  INTERFACE SigilVideoDecode SigilVideoEncode)
