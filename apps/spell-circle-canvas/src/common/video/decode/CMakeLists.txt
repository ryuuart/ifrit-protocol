if(APPLE)
  set(SIGIL_VIDEO_DEVICE_SOURCE DeviceMetal.mm)
else()
  set(SIGIL_VIDEO_DEVICE_SOURCE DeviceStub.cpp)
endif()

add_library(SigilVideoDevice STATIC
  ${SIGIL_VIDEO_DEVICE_SOURCE}
  Device.h
)
target_compile_features(SigilVideoDevice PUBLIC cxx_std_20)
target_include_directories(SigilVideoDevice PUBLIC ${SIGIL_VIDEO_INCLUDE_DIR})
target_link_libraries(SigilVideoDevice PUBLIC SigilVideoCore
  unofficial::skia::skia)
_sigil_video_ffmpeg(SigilVideoDevice)
if(APPLE)
  target_link_libraries(SigilVideoDevice PRIVATE
    "-framework CoreVideo" "-framework Metal")
endif()

add_library(SigilVideoDecode STATIC
  Decode.cpp
  Playback.cpp
  ${SIGIL_VIDEO_INCLUDE_DIR}/sigilvideo/decode/Decode.h
  ${SIGIL_VIDEO_INCLUDE_DIR}/sigilvideo/decode/Playback.h
)
target_compile_features(SigilVideoDecode PUBLIC cxx_std_20)
target_include_directories(SigilVideoDecode PUBLIC ${SIGIL_VIDEO_INCLUDE_DIR})
target_link_libraries(SigilVideoDecode PUBLIC SigilVideoCore
  unofficial::skia::skia PRIVATE SigilVideoDevice
  concurrentqueue::concurrentqueue)
_sigil_video_ffmpeg(SigilVideoDecode)

add_executable(video_decode_test test/DecodeTest.cpp)
target_link_libraries(video_decode_test PRIVATE SigilVideoDecode
  GTest::gtest_main)
target_compile_definitions(video_decode_test PRIVATE
  IFRIT_VIDEO_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/assets")
add_test(NAME video_decode_test COMMAND video_decode_test)

if(APPLE AND TARGET SigilSkiaGraphite)
  add_executable(video_device_test test/DeviceTest.mm)
  set_source_files_properties(test/DeviceTest.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_link_libraries(video_device_test PRIVATE
    SigilVideoDecode
    SigilVideoEncode
    SigilSkiaGraphite
    GTest::gtest_main
    "-framework Metal")
  add_test(NAME video_device_test COMMAND video_device_test)

  add_executable(video_device_bench bench/DeviceBench.mm)
  set_source_files_properties(bench/DeviceBench.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_link_libraries(video_device_bench PRIVATE
    SigilVideoDecode
    SigilVideoEncode
    SigilSkiaGraphite
    "-framework Metal")
  add_dependencies(benches video_device_bench)
endif()
