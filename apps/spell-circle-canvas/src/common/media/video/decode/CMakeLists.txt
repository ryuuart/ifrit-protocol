# Decode: FFmpeg demux and decode, seeking, the frame cache, the CPU
# executor and the device executor beside it — VideoToolbox pixel buffers
# wrapped as Metal textures on Apple platforms, a stub that answers nothing
# elsewhere — behind the private Device.h seam.
if(APPLE)
  set(_device_source DeviceMetal.mm)
else()
  set(_device_source DeviceStub.cpp)
endif()

find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMediaVideoDecode
  SOURCES Container.cpp Frames.cpp Raster.cpp Video.cpp Playback.cpp
          ${_device_source} Device.h DecodeInternal.h PlaybackInternal.h
  HEADERS ../Video.h
  PUBLIC SigilMediaCore
  PRIVATE concurrentqueue::concurrentqueue sigil_media_ffmpeg
  FRAMEWORKS CoreVideo Metal
  ARC)
# The device executor wraps its planes through SigilSkia, which is where
# a texture someone else owns becomes an image: nothing here spells a
# Graphite backend texture.
if(APPLE)
  target_link_libraries(SigilMediaVideoDecode PRIVATE SigilSkiaGraphite)
endif()

sigil_test(media_test
  SOURCES test/DecodeTest.cpp
  LIBRARIES SigilMediaVideoDecode)

if(APPLE)
  sigil_frameworks(metal Metal)

  sigil_test(media_test
    SOURCES test/DeviceTest.mm
    LIBRARIES SigilMediaVideoDecode SigilMediaVideoEncode SigilSkiaGraphite ${metal}
    SUITES VideoDevice LABELS gpu
    ARC)

  sigil_bench(media_bench
    SOURCES bench/DeviceBench.mm
    LIBRARIES SigilMediaVideoDecode SigilMediaVideoEncode SigilSkiaGraphite ${metal}
    ARC)
endif()
