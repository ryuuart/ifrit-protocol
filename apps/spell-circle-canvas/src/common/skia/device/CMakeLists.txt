# The device feature: a GPU device and its queue, owned or adopted, with
# textures and fences named by generation-checked handles and a
# destruction queue that waits out the frames in flight. The
# API-independent translation unit owns the tables and the queue; each
# backend implements the private interface in DeviceBackend.h — Metal on
# Apple, Vulkan everywhere, resolving its entry points from the loader at
# run time so nothing links Vulkan and a machine without a driver only
# reports that. Links the graphite feature so
# GraphiteContext::create(GpuDevice&) can reach both.
if(APPLE)
  set(SIGIL_SKIA_DEVICE_NATIVE_SOURCES GpuDeviceMetal.mm)
else()
  set(SIGIL_SKIA_DEVICE_NATIVE_SOURCES)
endif()

add_library(SigilSkiaDevice STATIC
  DeviceBackend.h
  GpuDevice.cpp
  GpuDeviceVulkan.cpp
  GraphiteOnDevice.cpp
  ${SIGIL_SKIA_DEVICE_NATIVE_SOURCES}
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/device/Fence.h
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/device/GpuDevice.h
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/device/Handle.h
)
target_compile_features(SigilSkiaDevice PUBLIC cxx_std_20)
target_include_directories(SigilSkiaDevice PUBLIC ${SIGIL_SKIA_INCLUDE_DIR})
target_link_libraries(SigilSkiaDevice
  PUBLIC SigilSkiaGraphite
  PRIVATE Vulkan::Headers ${CMAKE_DL_LIBS})
if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(SigilSkiaDevice PRIVATE ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK})
endif()

# Needs a GPU, so the test and the benchmark exist only where one is
# guaranteed. Both compile without ARC: they count retains to prove
# ownership, and ARC would hide the counts they assert on.
if(APPLE)
  add_executable(sigilskia_device_test test/DeviceTest.mm)
  target_link_libraries(sigilskia_device_test PRIVATE SigilSkiaDevice
    GTest::gtest_main ${METAL_FRAMEWORK})
  add_test(NAME sigilskia_device_test COMMAND sigilskia_device_test)

  # Google Benchmark (use a Release build), linking only what it measures.
  # Not a test: it builds through the `benches` target and runs through
  # scripts/bench_ledger.py.
  add_executable(sigilskia_device_bench bench/DeviceBench.mm)
  target_link_libraries(sigilskia_device_bench PRIVATE SigilSkiaDevice
    benchmark::benchmark ${METAL_FRAMEWORK})
  add_dependencies(benches sigilskia_device_bench)
endif()
