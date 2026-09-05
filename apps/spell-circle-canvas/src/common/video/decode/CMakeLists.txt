# Decode: FFmpeg demux and decode, seeking, the frame cache, the CPU
# executor and the device executor beside it — VideoToolbox pixel buffers
# wrapped as Metal textures on Apple platforms, a stub that answers nothing
# elsewhere — behind the private Device.h seam.
if(APPLE)
  set(_device_source DeviceMetal.mm)
  set_source_files_properties(DeviceMetal.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
else()
  set(_device_source DeviceStub.cpp)
endif()

if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
endif()
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilVideoDecode
  SOURCES Decode.cpp Playback.cpp ${_device_source} Device.h
  HEADERS Decode.h Playback.h
  PUBLIC SigilVideoCore unofficial::skia::skia
  PRIVATE concurrentqueue::concurrentqueue sigil_video_ffmpeg)
# The device executor wraps its planes through SigilSkia, which is where
# a texture someone else owns becomes an image: nothing here spells a
# Graphite backend texture.
if(APPLE)
  target_link_libraries(SigilVideoDecode PRIVATE
    SigilSkiaGraphite "-framework CoreVideo" ${METAL_FRAMEWORK})
endif()

# The committed clips sit beside this feature's test rather than at the
# library root.
sigil_test(video_decode_test
  SOURCES test/DecodeTest.cpp
  LIBRARIES SigilVideoDecode
  ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/assets)

if(APPLE)
  sigil_test(video_device_test
    SOURCES test/DeviceTest.mm
    LIBRARIES SigilVideoDecode SigilVideoEncode SigilSkiaGraphite
              ${METAL_FRAMEWORK}
    LABELS gpu
    ARC)

  sigil_bench(video_device_bench
    SOURCES bench/DeviceBench.mm
    LIBRARIES SigilVideoDecode SigilVideoEncode SigilSkiaGraphite
              ${METAL_FRAMEWORK}
    ARC)
endif()
