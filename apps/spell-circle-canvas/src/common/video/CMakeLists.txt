# SigilVideo — streaming video decode and encode over FFmpeg, with a
# device executor for native decoded frames and a CPU executor for every
# surface that can accept pixels.

set(SIGIL_VIDEO_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

find_package(concurrentqueue CONFIG REQUIRED)

function(_sigil_video_ffmpeg target)
  target_include_directories(${target} PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_directories(${target} PRIVATE ${FFMPEG_LIBRARY_DIRS})
  target_link_libraries(${target} PRIVATE ${FFMPEG_LIBRARIES})
endfunction()

add_library(SigilVideoCore INTERFACE
  ${SIGIL_VIDEO_INCLUDE_DIR}/sigilvideo/Types.h
)
target_compile_features(SigilVideoCore INTERFACE cxx_std_20)
target_include_directories(SigilVideoCore INTERFACE ${SIGIL_VIDEO_INCLUDE_DIR})

add_subdirectory(decode)
add_subdirectory(encode)

add_library(SigilVideo INTERFACE
  ${SIGIL_VIDEO_INCLUDE_DIR}/sigilvideo/Video.h
)
target_link_libraries(SigilVideo INTERFACE SigilVideoDecode SigilVideoEncode)

sigil_add_docs(
  NAME SigilVideo
  BRIEF "Streaming video decoding, device composition, and video encoding"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
