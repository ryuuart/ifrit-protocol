# SigilScry — HTML/CSS/JS layout for the scene canvases, rendered by the
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

add_library(SigilScry STATIC
  WebEngine.cpp
  WebView.cpp
  WebImage.cpp
  UltralightPlatform.cpp
  WebInternal.h
  WebGpuDriver.h
  include/sigilscry/WebEngine.h
  include/sigilscry/WebView.h
  include/sigilscry/WebImage.h
  ${IFRIT_WEB_METAL_SOURCES}
)

if(APPLE)
  set_source_files_properties(metal/UltralightMetalDriver.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_include_directories(SigilScry PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    ${CMAKE_CURRENT_LIST_DIR}/metal
  )
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(SigilScry PRIVATE ${METAL_FRAMEWORK})
endif()

target_compile_features(SigilScry PUBLIC cxx_std_20)

target_include_directories(SigilScry PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilScry
  PUBLIC
    unofficial::skia::skia
  PRIVATE
    Ultralight::Ultralight
    Ultralight::AppCore
)

# Fallback resource location (icudt67l.dat, cacert.pem) compiled into the
# engine; WebEngineConfig::resourceDir overrides it at runtime.
target_compile_definitions(SigilScry PRIVATE
  IFRIT_WEB_DEFAULT_RESOURCE_DIR="${Ultralight_RESOURCE_DIR}"
)

add_executable(scry_test
  test/WebViewTest.cpp
)

target_link_libraries(scry_test PRIVATE SigilScry GTest::gtest_main)
ultralight_copy_resources(scry_test)

add_test(NAME scry_test COMMAND scry_test)

# Headless demo: renders HTML layouts through SigilScry and composites them
# with direct SkCanvas drawing into PNGs (out dir: first argument,
# default ./scry_demo_out).
add_executable(scry_demo
  examples/scry_demo.cpp
)

target_link_libraries(scry_demo PRIVATE SigilScry)
ultralight_copy_resources(scry_demo)

# Integration-path benchmarks; run `scry_bench` for the CPU engine and
# `scry_bench --gpu` for the GPU engine (one Ultralight renderer per
# process, so the two modes are separate runs).
if(NOT APPLE OR TARGET SpellCircleSkia)
  if(APPLE)
    set(IFRIT_WEB_BENCH_GPU_SOURCES bench/BenchGpuMetal.mm)
  else()
    set(IFRIT_WEB_BENCH_GPU_SOURCES bench/BenchGpuStub.cpp)
  endif()
  add_executable(scry_bench
    bench/WebBench.cpp
    bench/BenchGpu.h
    ${IFRIT_WEB_BENCH_GPU_SOURCES}
  )
  target_link_libraries(scry_bench PRIVATE SigilScry benchmark::benchmark)
  if(APPLE)
    set_source_files_properties(bench/BenchGpuMetal.mm PROPERTIES
      COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(scry_bench PRIVATE SpellCircleSkia ${METAL_FRAMEWORK})
  endif()
  ultralight_copy_resources(scry_bench)
endif()

# GPU-mode targets: Ultralight's Metal GPUDriver feeding texture-backed
# SkImages into a Graphite context (SpellCircleSkia provides the Qt-free
# Metal bring-up). Separate test binary because Ultralight allows one
# Renderer per process and scry_test owns the CPU-mode engine.
if(APPLE AND TARGET SpellCircleSkia)
  add_executable(scry_gpu_test
    test/WebViewGpuTest.mm
  )
  target_link_libraries(scry_gpu_test PRIVATE
    SigilScry SpellCircleSkia GTest::gtest_main ${METAL_FRAMEWORK})
  ultralight_copy_resources(scry_gpu_test)
  add_test(NAME scry_gpu_test COMMAND scry_gpu_test)

  add_executable(scry_gpu_demo
    examples/scry_gpu_demo.mm
  )
  target_link_libraries(scry_gpu_demo PRIVATE
    SigilScry SpellCircleSkia ${METAL_FRAMEWORK})
  ultralight_copy_resources(scry_gpu_demo)
endif()
