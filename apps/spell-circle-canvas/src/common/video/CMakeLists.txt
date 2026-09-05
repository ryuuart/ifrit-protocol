# SigilVideo — streaming video decode and encode over FFmpeg, with a
# device executor for native decoded frames and a CPU executor for every
# surface that can accept pixels.
sigil_library_root(SigilVideo
  BRIEF "Streaming video decoding, device composition, and video encoding")

find_package(concurrentqueue CONFIG REQUIRED)

# The ffmpeg port publishes a find module and the include, library and
# link-directory variables it fills — no package, no target — so the one
# target the features link is declared here from those variables. The
# module resolves the release and debug archives as a pair, which the
# pkg-config files beside them cannot: one .pc describes one build.
# The variables name the release and the debug archive of each library
# as a `optimized;<a>;debug;<b>` pair, which only target_link_libraries
# reads, so the interface is built with the commands and not by setting
# the properties.
add_library(sigil_video_ffmpeg INTERFACE)
target_include_directories(sigil_video_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(sigil_video_ffmpeg INTERFACE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(sigil_video_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})

sigil_library(SigilVideoCore
  HEADERS Types.h)

add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilVideo
  HEADERS Video.h
  INTERFACE SigilVideoDecode SigilVideoEncode)
