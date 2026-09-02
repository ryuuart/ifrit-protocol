# The hardware feature: a GPU device and its one command queue, owned or
# adopted, with the resources that live on it named by generation-checked
# handles, a mip chain where a description asks for one, a destruction
# queue that waits out the frames in flight, and fences as timelines.
#
# NOTHING HERE KNOWS WHAT DRAWS. No Skia, no Diligent, no Qt: the value is
# the device itself, so a drawing library stands on it rather than owning
# it, and two of them can stand on the same one. This is the one feature
# in SigilCore that is not the standard library alone — it is the graphics
# API and no more — and only a consumer that needs a device links it.
#
# The API-independent translation unit owns the handle tables and the
# queue; each backend implements the private interface in DeviceBackend.h
# — Metal on Apple, Vulkan everywhere, resolving its entry points from the
# loader at run time so nothing links Vulkan and a machine without a
# driver only reports that.
if(APPLE)
  set(SIGIL_CORE_HARDWARE_NATIVE_SOURCES GpuDeviceMetal.mm)
else()
  set(SIGIL_CORE_HARDWARE_NATIVE_SOURCES)
endif()

find_package(VulkanHeaders CONFIG REQUIRED)

add_library(SigilCoreHardware STATIC
  DeviceBackend.h
  GpuDevice.cpp
  GpuDeviceVulkan.cpp
  ${SIGIL_CORE_HARDWARE_NATIVE_SOURCES}
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/hardware/Fence.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/hardware/GpuDevice.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/hardware/Handle.h
)
target_compile_features(SigilCoreHardware PUBLIC cxx_std_20)
target_include_directories(SigilCoreHardware PUBLIC ${SIGIL_CORE_INCLUDE_DIR})
# Vulkan's headers are private: every public header spells a Vulkan handle
# as an opaque value, so no consumer needs them.
target_link_libraries(SigilCoreHardware PRIVATE Vulkan::Headers
  ${CMAKE_DL_LIBS})
if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(SigilCoreHardware PRIVATE ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK})
endif()

# Needs a GPU, so the test and the benchmark exist only where one is
# guaranteed. Both compile without ARC: they count retains to prove
# ownership, and ARC would hide the counts they assert on.
if(APPLE)
  add_executable(core_hardware_test test/DeviceTest.mm)
  target_link_libraries(core_hardware_test PRIVATE SigilCoreHardware
    GTest::gtest_main ${METAL_FRAMEWORK})
  add_test(NAME core_hardware_test COMMAND core_hardware_test)

  # Google Benchmark (use a Release build), linking only what it measures.
  # Not a test: it builds through the `benches` target and runs through
  # scripts/bench_ledger.py.
  add_executable(core_hardware_bench bench/DeviceBench.mm)
  target_link_libraries(core_hardware_bench PRIVATE SigilCoreHardware
    benchmark::benchmark ${METAL_FRAMEWORK})
  add_dependencies(benches core_hardware_bench)
endif()
