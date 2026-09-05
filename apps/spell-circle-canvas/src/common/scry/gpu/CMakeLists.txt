# The gpu feature: Ultralight's GPU command lists executed on the
# device behind a SigilCoreHardware GpuDevice, and the texture interop the
# engine needs beyond that — publish and slot textures named by handle,
# blits, uploads, painting through a Graphite recorder, and wrapping a
# texture as an SkImage. GpuDriver.h is the graphics-API-neutral
# contract; metal/ is its one implementation, and a Vulkan one joins
# beside it without the engine noticing. Both headers name
# Ultralight and stay beside the sources.
#
# Textures and fences are named on SigilCoreHardware's device and the
# driver paints through SigilSkia's Graphite context: both PUBLIC, because
# the driver contract spells the handle and the context.
if(APPLE)
  # The SDK's stock Metal shader source, embedded so the driver compiles
  # it at runtime on whatever device the host passes in.
  file(READ "${CMAKE_CURRENT_LIST_DIR}/metal/UltralightShaders.metal"
       SIGILSCRY_ULTRALIGHT_SHADER_SOURCE)
  configure_file(metal/ShaderSource.h.in
                 "${CMAKE_CURRENT_BINARY_DIR}/generated/ShaderSource.h"
                 @ONLY)
  set(SIGIL_SCRY_GPU_NATIVE_SOURCES
      metal/MetalDriver.h
      metal/MetalDriver.mm
      metal/MetalDriverState.h
      metal/MetalInterop.mm)
else()
  set(SIGIL_SCRY_GPU_NATIVE_SOURCES)
endif()

if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
endif()
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilScryGpu
  SOURCES GpuDriver.h ${SIGIL_SCRY_GPU_NATIVE_SOURCES}
  PUBLIC SigilSkiaGraphite SigilCoreHardware Boost::unordered
  PRIVATE Ultralight::Ultralight)
if(APPLE)
  set_source_files_properties(metal/MetalDriver.mm metal/MetalInterop.mm
    PROPERTIES COMPILE_OPTIONS -fobjc-arc)
  target_include_directories(SigilScryGpu PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_link_libraries(SigilScryGpu PRIVATE ${METAL_FRAMEWORK})

  # Needs a GPU, so the test and the benchmark exist only where one is
  # guaranteed. Both drive the driver directly — no renderer, no page —
  # so they share a process with nothing and prove the interop alone.
  # Every case needs a device, so on a machine without one the binary
  # checks nothing; the label makes that visible as a lane not run rather
  # than as a lane that passed.
  sigil_test(scry_test
    SOURCES test/GpuDriverTest.mm
    SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
    LIBRARIES SigilScryGpu Ultralight::Ultralight ${METAL_FRAMEWORK}
    SUITES ScryGpuDriver LABELS gpu
    ARC)

  sigil_bench(scry_bench
    SOURCES bench/GpuDriverBench.mm
    SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
    LIBRARIES SigilScryGpu Ultralight::Ultralight ${METAL_FRAMEWORK}
    ARC)
endif()
