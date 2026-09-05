# SigilVideo — streaming video decode and encode over FFmpeg, with a
# device executor for native decoded frames and a CPU executor for every
# surface that can accept pixels.
sigil_library_root(SigilVideo
  BRIEF "Streaming video decoding, device composition, and video encoding")

find_package(concurrentqueue CONFIG REQUIRED)

# FFmpeg comes as include and library directories rather than a target,
# which is the one link the library helper cannot spell.
function(_sigil_video_ffmpeg target)
  target_include_directories(${target} PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_directories(${target} PRIVATE ${FFMPEG_LIBRARY_DIRS})
  target_link_libraries(${target} PRIVATE ${FFMPEG_LIBRARIES})
endfunction()

sigil_library(SigilVideoCore
  HEADERS Types.h)

add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilVideo
  HEADERS Video.h
  INTERFACE SigilVideoDecode SigilVideoEncode)
