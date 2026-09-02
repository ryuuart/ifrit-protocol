# The gpu feature: Ultralight's GPU command lists executed on the
# device behind a SigilCoreHardware GpuDevice, and the texture interop the
# engine needs beyond that — publish and slot textures named by handle,
# blits, uploads, painting through a Graphite recorder, and wrapping a
# texture as an SkImage. GpuDriver.h is the graphics-API-neutral
# contract; metal/ is its one implementation today, and a Vulkan one
# joins beside it without the engine noticing. Both headers name
# Ultralight and stay beside the sources.
if(APPLE)
  # The SDK's stock Metal shader source, embedded so the driver compiles
  # it at runtime on whatever device the host passes in.
  file(READ "${CMAKE_CURRENT_LIST_DIR}/metal/UltralightShaders.metal"
       IFRIT_WEB_ULTRALIGHT_SHADER_SOURCE)
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

add_library(SigilScryGpu STATIC
  GpuDriver.h
  ${SIGIL_SCRY_GPU_NATIVE_SOURCES}
)
target_compile_features(SigilScryGpu PUBLIC cxx_std_20)
# Textures and fences are named on SigilCoreHardware's device and the
# driver paints through SigilSkia's Graphite context: both PUBLIC, because
# the driver contract spells the handle and the context.
target_link_libraries(SigilScryGpu
  PUBLIC SigilSkiaGraphite SigilCoreHardware
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
  add_executable(scry_gpu_test test/GpuDriverTest.mm)
  set_source_files_properties(test/GpuDriverTest.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  # The library's own test directory holds the device, the Graphite
  # context and the asynchronous read every GPU test here shares.
  target_include_directories(scry_gpu_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../test)
  target_link_libraries(scry_gpu_test PRIVATE SigilScryGpu
    Ultralight::Ultralight GTest::gtest_main ${METAL_FRAMEWORK})
  add_test(NAME scry_gpu_test COMMAND scry_gpu_test)
  # Every case here needs a device, so on a machine without one the
  # binary checks nothing. The label is what makes that visible as a
  # lane not run rather than as a lane that passed.
  set_tests_properties(scry_gpu_test PROPERTIES LABELS gpu)

  # Google Benchmark (use a Release build). Not a test: it builds through
  # the `benches` target and runs through scripts/bench_ledger.py.
  add_executable(scry_gpu_bench bench/GpuDriverBench.mm)
  set_source_files_properties(bench/GpuDriverBench.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_include_directories(scry_gpu_bench PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  target_link_libraries(scry_gpu_bench PRIVATE SigilScryGpu
    Ultralight::Ultralight benchmark::benchmark ${METAL_FRAMEWORK})
  add_dependencies(benches scry_gpu_bench)
endif()
