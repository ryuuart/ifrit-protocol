# IfritWeb — HTML/CSS/JS layout for the scene canvases, rendered by the
# Ultralight SDK (WebKit-derived, CPU renderer) into premultiplied-BGRA
# SkImage frames. A dedicated web thread drives ultralight::Renderer and
# publishes immutable snapshots that any SkCanvas — raster or Graphite —
# can draw like an IfritImage frame; an unthreaded mode lets a host drive
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

add_library(IfritWeb STATIC
  WebEngine.cpp
  WebView.cpp
  WebImage.cpp
  UltralightPlatform.cpp
  WebInternal.h
  include/ifritweb/WebEngine.h
  include/ifritweb/WebView.h
  include/ifritweb/WebImage.h
  ${IFRIT_WEB_METAL_SOURCES}
)

if(APPLE)
  set_source_files_properties(metal/UltralightMetalDriver.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_include_directories(IfritWeb PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    ${CMAKE_CURRENT_LIST_DIR}/metal
  )
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(IfritWeb PRIVATE ${METAL_FRAMEWORK})
endif()

target_compile_features(IfritWeb PUBLIC cxx_std_20)

target_include_directories(IfritWeb PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(IfritWeb
  PUBLIC
    unofficial::skia::skia
  PRIVATE
    Ultralight::Ultralight
    Ultralight::AppCore
)

# Fallback resource location (icudt67l.dat, cacert.pem) compiled into the
# engine; WebEngineConfig::resourceDir overrides it at runtime.
target_compile_definitions(IfritWeb PRIVATE
  IFRIT_WEB_DEFAULT_RESOURCE_DIR="${Ultralight_RESOURCE_DIR}"
)

add_executable(web_test
  test/WebViewTest.cpp
)

target_link_libraries(web_test PRIVATE IfritWeb GTest::gtest_main)
ultralight_copy_resources(web_test)

add_test(NAME web_test COMMAND web_test)

# Headless demo: renders HTML layouts through IfritWeb and composites them
# with direct SkCanvas drawing into PNGs (out dir: first argument,
# default ./ifritweb_demo_out).
add_executable(ifritweb_demo
  examples/ifritweb_demo.cpp
)

target_link_libraries(ifritweb_demo PRIVATE IfritWeb)
ultralight_copy_resources(ifritweb_demo)

# GPU-mode targets: Ultralight's Metal GPUDriver feeding texture-backed
# SkImages into a Graphite context (SpellCircleSkia provides the Qt-free
# Metal bring-up). Separate test binary because Ultralight allows one
# Renderer per process and web_test owns the CPU-mode engine.
if(APPLE AND TARGET SpellCircleSkia)
  add_executable(web_gpu_test
    test/WebViewGpuTest.mm
  )
  target_link_libraries(web_gpu_test PRIVATE
    IfritWeb SpellCircleSkia GTest::gtest_main ${METAL_FRAMEWORK})
  ultralight_copy_resources(web_gpu_test)
  add_test(NAME web_gpu_test COMMAND web_gpu_test)

  add_executable(ifritweb_gpu_demo
    examples/ifritweb_gpu_demo.mm
  )
  target_link_libraries(ifritweb_gpu_demo PRIVATE
    IfritWeb SpellCircleSkia ${METAL_FRAMEWORK})
  ultralight_copy_resources(ifritweb_gpu_demo)
endif()
