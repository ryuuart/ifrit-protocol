# The diligent feature: the one GPU device 2D and 3D share. Diligent
# creates the Vulkan device — this build of it cannot attach to a device
# that already exists, and has no Metal backend — and SigilSkia adopts
# the device, queue and loader entry points it made, so there is one
# device, one queue and one handle table under both APIs.
#
# VolkShim.c supplies the volk loader the diligent-engine port expects
# (vendored under thirdparty/volk, MIT), with a Homebrew-aware
# volkInitialize so /opt/homebrew MoltenVK installs work without DYLD
# environment surgery. It is the loader for Diligent alone: SigilSkia
# resolves its own entry points and links nothing, and AdoptDevice.cpp
# passes volk's vkGetInstanceProcAddr on to it so both APIs dispatch
# through the one loader this process opened.
enable_language(C)

find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

add_library(SigilWorldDiligent STATIC
  AdoptDevice.cpp
  AdoptDevice.h
  Device.cpp
  VolkShim.c
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Device.h
)
target_compile_features(SigilWorldDiligent PUBLIC cxx_std_20)
target_include_directories(SigilWorldDiligent
  PUBLIC ${SIGIL_WORLD_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)

# SigilSkiaDevice is PUBLIC because Device.h hands out the GpuDevice and
# the Graphite context standing on it, and a caller that uses either
# needs their headers. Diligent is PRIVATE: the public header names its
# two interfaces by forward declaration only.
target_link_libraries(SigilWorldDiligent
  PUBLIC SigilSkiaDevice
  PRIVATE unofficial::diligent-engine::DiligentCore Vulkan::Headers)

# The engine's Vulkan surface code references the Metal/MoltenVK WSI
# entry points, which volk only emits when the platform extensions are
# visible.
if(APPLE)
  set_source_files_properties(VolkShim.c PROPERTIES COMPILE_DEFINITIONS
    "VK_USE_PLATFORM_METAL_EXT=1;VK_USE_PLATFORM_MACOS_MVK=1")
endif()

# Links Diligent directly: proving that Diligent still drives the device
# after Graphite has submitted on it means issuing a Diligent command.
add_executable(world_diligent_test test/DeviceTest.cpp)
target_link_libraries(world_diligent_test PRIVATE SigilWorldDiligent
  unofficial::diligent-engine::DiligentCore GTest::gtest_main)
add_test(NAME world_diligent_test COMMAND world_diligent_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_diligent_bench bench/DeviceBench.cpp)
target_link_libraries(world_diligent_bench PRIVATE SigilWorldDiligent
  benchmark::benchmark)
add_dependencies(benches world_diligent_bench)
