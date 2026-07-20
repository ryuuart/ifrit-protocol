# IfritCompose — data-driven, cacheable, animated drawable components
# over Yoga + TextFlow + Choreograph/IfritTick + Skia. DESIGN.md is the
# architecture, API.md the surface, STRESS_TESTS.md the acceptance
# catalog. This is the phase-1 kernel: element values, keyed
# reconciler, Yoga layout with TextFlow-measured text, stacking paint,
# automatic picture caching of provably-static subtrees, and
# Choreograph transitions/bindings.
add_library(IfritCompose STATIC
  Compose.cpp
  Composer.cpp
  ComposeInternal.h
  include/ifritcompose/Compose.h
  include/ifritcompose/Decorations.h
)

target_compile_features(IfritCompose PUBLIC cxx_std_20)

target_include_directories(IfritCompose PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(IfritCompose
  PUBLIC
    IfritImage
    IfritTick
    TextFlow
    unofficial::skia::skia
  PRIVATE
    yoga::yogacore
)

add_executable(compose_test
  test/ComposeTest.cpp
)
target_link_libraries(compose_test PRIVATE
  IfritCompose TextFlowPorts GTest::gtest_main)
add_test(NAME compose_test COMMAND compose_test)

# The pre-implementation feasibility spike stays: it pins the
# Yoga+TextFlow measurement contract independently of the kernel.
add_executable(compose_spike_test
  test/ComposeSpikeTest.cpp
)
target_link_libraries(compose_spike_test PRIVATE
  TextFlow TextFlowPorts yoga::yogacore GTest::gtest_main)
add_test(NAME compose_spike_test COMMAND compose_spike_test)

# Perf gate (run in Release): describe/reconcile/draw costs, memo hit
# behavior, cache effectiveness. Numbers land in STRESS_TESTS.md.
if(APPLE AND TARGET SpellCircleSkia)
  set(COMPOSE_BENCH_GPU_SOURCES
      bench/ComposeBenchGpuMetal.mm bench/ComposeBenchGpu.h)
else()
  set(COMPOSE_BENCH_GPU_SOURCES)
endif()
add_executable(compose_bench
  bench/ComposeBench.cpp
  ${COMPOSE_BENCH_GPU_SOURCES}
)
target_link_libraries(compose_bench PRIVATE
  IfritCompose TextFlowPorts benchmark::benchmark EnTT::EnTT)
if(APPLE AND TARGET SpellCircleSkia)
  set_source_files_properties(bench/ComposeBenchGpuMetal.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_compile_definitions(compose_bench PRIVATE COMPOSE_BENCH_GRAPHITE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  target_link_libraries(compose_bench PRIVATE
    SpellCircleSkia ${METAL_FRAMEWORK})
endif()

# Headless gallery: renders stress-catalog panels to PNG (out dir:
# first argument, default ./compose_demo_out). The interactive Qt
# ComposeGallery follows in a later phase.
add_executable(compose_demo
  examples/compose_demo.cpp
)
target_link_libraries(compose_demo PRIVATE IfritCompose TextFlowPorts)

# ComposeGallery — Qt Quick app in the TextFlowGallery mold: QML
# sidebar over the scene registry (every STRESS_TESTS.md gallery item,
# tagged), clock controls, and a live frame-metrics panel.
# `--headless <outdir>` prints the per-scene FPS table and captures.
qt_add_executable(ComposeGallery
  gallery/gallery_main.cpp
  gallery/GalleryScenes.h
)
qt_add_qml_module(ComposeGallery
  URI Compose.Gallery
  VERSION 1.0
  QML_FILES gallery/qml/Main.qml
  SOURCES gallery/ComposeGalleryView.cpp gallery/ComposeGalleryView.h
)
set_source_files_properties(gallery/qml/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml)
target_include_directories(ComposeGallery PRIVATE gallery)
target_link_libraries(ComposeGallery PRIVATE
  IfritCompose TextFlowPorts Qt6::Gui Qt6::Quick EnTT::EnTT)

# ComposeSketch — the p5-style live-coding host (cr.h/RCC++ pattern):
# sketches are real C++ over the full compose API, compiled on save
# into dylibs that resolve the framework's symbols from the host
# executable (-undefined dynamic_lookup + -export_dynamic), so a sketch
# build is one small TU with no linking against the static libraries.
#
# The kit is a proper library: its PUBLIC deps define what sketches may
# use, and SketchAnchor.cpp compiles the sketch prelude inside the real
# target graph — ExtractSketchFlags.cmake lifts that TU's command from
# compile_commands.json into sketch_flags.rsp at build time, so sketch
# compilation has no hand-maintained flag list.
add_library(IfritSketchKit STATIC
  sketch/SketchHost.cpp
  sketch/SketchHost.h
  sketch/Assets.cpp
  sketch/SketchAnchor.cpp
)
target_include_directories(IfritSketchKit PUBLIC sketch/include)
target_link_libraries(IfritSketchKit PUBLIC
  IfritCompose TextFlowPorts EnTT::EnTT)
set_target_properties(IfritSketchKit PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

qt_add_executable(ComposeSketch
  sketch/sketch_main.cpp
)
qt_add_qml_module(ComposeSketch
  URI Compose.Sketch
  VERSION 1.0
  QML_FILES sketch/qml/Main.qml
  SOURCES sketch/ComposeSketchView.cpp sketch/ComposeSketchView.h
)
set_source_files_properties(sketch/qml/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml)
target_include_directories(ComposeSketch PRIVATE sketch)
target_link_libraries(ComposeSketch PRIVATE
  IfritSketchKit Qt6::Gui Qt6::Quick)
if(APPLE)
  target_link_options(ComposeSketch PRIVATE "LINKER:-export_dynamic")
  # Sketches may call ANY part of the SDK, not just what the host's own
  # TUs reference — force the full archives into the executable so
  # -undefined dynamic_lookup always resolves. (Skia is the exception:
  # vcpkg builds it hidden-visibility, so sketches link its archive
  # directly — see EXTRA below.)
  foreach(sketch_dep IN ITEMS IfritSketchKit IfritCompose IfritImage
                              IfritTick TextFlow TextFlowPorts)
    target_link_options(ComposeSketch PRIVATE
      "LINKER:-force_load,$<TARGET_FILE:${sketch_dep}>")
  endforeach()
else()
  # Non-Apple ports: mirror with --whole-archive when the sketch host
  # lands there (see the repo's other platform-draft notes).
  target_link_options(ComposeSketch PRIVATE "LINKER:--export-dynamic")
endif()

add_custom_command(TARGET ComposeSketch POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DCOMPDB=${CMAKE_BINARY_DIR}/compile_commands.json
    -DANCHOR=${CMAKE_CURRENT_LIST_DIR}/sketch/SketchAnchor.cpp
    -DCONFIG=$<CONFIG>
    -DOUT=$<TARGET_FILE_DIR:ComposeSketch>/sketch_flags.rsp
    -DEXTRA=$<TARGET_FILE:unofficial::skia::skia>
    -P ${CMAKE_SOURCE_DIR}/cmake/ExtractSketchFlags.cmake
  COMMENT "Extracting sketch compile flags from compile_commands.json"
  VERBATIM)

# End-to-end smoke test of the reload pipeline: compile the starter
# sketch through the same flags, dlopen it, render a frame.
add_test(NAME compose_sketch_smoke
  COMMAND ComposeSketch
    ${CMAKE_CURRENT_LIST_DIR}/sketch/sketches/hello.cpp
    --frame ${CMAKE_BINARY_DIR}/compose_sketch_smoke.png)
