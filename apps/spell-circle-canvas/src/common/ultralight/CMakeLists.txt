# SigilUltralight — HTML/CSS/JS layout for the scene canvases, rendered by the
# Ultralight SDK (WebKit-derived, CPU renderer) into premultiplied-BGRA
# SkImage frames. A dedicated web thread drives ultralight::Renderer and
# publishes immutable snapshots that any SkCanvas — raster or Graphite —
# can draw like an SigilImage frame; an unthreaded mode lets a host drive
# rendering in lockstep with its own loop instead. Only added when
# SPELLCIRCLE_ENABLE_ULTRALIGHT is ON (auto-off when the SDK is missing;
# see cmake/FindUltralight.cmake).
if(APPLE)
  # Embed the SDK's stock Metal shader source (vendored as
  # metal/UltralightShaders.metal) so the driver can compile it at runtime
  # on whatever device the host passes in.
  file(READ "${CMAKE_CURRENT_LIST_DIR}/metal/UltralightShaders.metal"
       IFRIT_WEB_ULTRALIGHT_SHADER_SOURCE)
  configure_file(metal/UltralightShaderSource.h.in
                 "${CMAKE_CURRENT_BINARY_DIR}/generated/UltralightShaderSource.h"
                 @ONLY)
  set(IFRIT_WEB_METAL_SOURCES
      metal/UltralightMetalDriver.mm
      metal/UltralightMetalDriver.h)
else()
  set(IFRIT_WEB_METAL_SOURCES)
endif()

add_library(SigilUltralight STATIC
  WebEngine.cpp
  WebView.cpp
  WebImage.cpp
  UltralightPlatform.cpp
  WebInternal.h
  WebGpuDriver.h
  include/sigilultralight/WebEngine.h
  include/sigilultralight/WebView.h
  include/sigilultralight/WebImage.h
  ${IFRIT_WEB_METAL_SOURCES}
)

if(APPLE)
  set_source_files_properties(metal/UltralightMetalDriver.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_include_directories(SigilUltralight PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    ${CMAKE_CURRENT_LIST_DIR}/metal
  )
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(SigilUltralight PRIVATE ${METAL_FRAMEWORK})
endif()

target_compile_features(SigilUltralight PUBLIC cxx_std_20)

target_include_directories(SigilUltralight PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilUltralight
  PUBLIC
    unofficial::skia::skia
  PRIVATE
    Ultralight::Ultralight
    Ultralight::AppCore
)

# Fallback resource location (icudt67l.dat, cacert.pem) compiled into the
# engine; WebEngineConfig::resourceDir overrides it at runtime.
target_compile_definitions(SigilUltralight PRIVATE
  IFRIT_WEB_DEFAULT_RESOURCE_DIR="${Ultralight_RESOURCE_DIR}"
)

add_executable(ultralight_test
  test/WebViewTest.cpp
)

target_link_libraries(ultralight_test PRIVATE SigilUltralight GTest::gtest_main)
ultralight_copy_resources(ultralight_test)

add_test(NAME ultralight_test COMMAND ultralight_test)

# Headless demo: renders HTML layouts through SigilUltralight and composites them
# with direct SkCanvas drawing into PNGs (out dir: first argument,
# default ./ultralight_demo_out).
add_executable(ultralight_demo
  examples/ultralight_demo.cpp
)

target_link_libraries(ultralight_demo PRIVATE SigilUltralight)
ultralight_copy_resources(ultralight_demo)

# Integration-path benchmarks; run `ultralight_bench` for the CPU engine and
# `ultralight_bench --gpu` for the GPU engine (one Ultralight renderer per
# process, so the two modes are separate runs).
if(NOT APPLE OR TARGET SpellCircleSkia)
  if(APPLE)
    set(IFRIT_WEB_BENCH_GPU_SOURCES bench/BenchGpuMetal.mm)
  else()
    set(IFRIT_WEB_BENCH_GPU_SOURCES bench/BenchGpuStub.cpp)
  endif()
  add_executable(ultralight_bench
    bench/WebBench.cpp
    bench/BenchGpu.h
    ${IFRIT_WEB_BENCH_GPU_SOURCES}
  )
  target_link_libraries(ultralight_bench PRIVATE SigilUltralight benchmark::benchmark)
  if(APPLE)
    set_source_files_properties(bench/BenchGpuMetal.mm PROPERTIES
      COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(ultralight_bench PRIVATE SpellCircleSkia ${METAL_FRAMEWORK})
  endif()
  ultralight_copy_resources(ultralight_bench)
endif()

# GPU-mode targets: Ultralight's Metal GPUDriver feeding texture-backed
# SkImages into a Graphite context (SpellCircleSkia provides the Qt-free
# Metal bring-up). Separate test binary because Ultralight allows one
# Renderer per process and ultralight_test owns the CPU-mode engine.
if(APPLE AND TARGET SpellCircleSkia)
  add_executable(ultralight_gpu_test
    test/WebViewGpuTest.mm
  )
  target_link_libraries(ultralight_gpu_test PRIVATE
    SigilUltralight SpellCircleSkia GTest::gtest_main ${METAL_FRAMEWORK})
  ultralight_copy_resources(ultralight_gpu_test)
  add_test(NAME ultralight_gpu_test COMMAND ultralight_gpu_test)

  add_executable(ultralight_gpu_demo
    examples/ultralight_gpu_demo.mm
  )
  target_link_libraries(ultralight_gpu_demo PRIVATE
    SigilUltralight SpellCircleSkia ${METAL_FRAMEWORK})
  ultralight_copy_resources(ultralight_gpu_demo)
endif()
