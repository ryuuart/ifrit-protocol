# SigilCompose — data-driven, cacheable, animated drawable components
# over Yoga + SigilWeave + Choreograph/SigilMotion + Skia. DESIGN.md is the
# architecture, API.md the surface, STRESS_TESTS.md the acceptance
# catalog. This is the phase-1 kernel: element values, keyed
# reconciler, Yoga layout with SigilWeave-measured text, stacking paint,
# automatic picture caching of provably-static subtrees, and
# Choreograph transitions/bindings.
add_library(SigilCompose STATIC
  Compose.cpp
  Composer.cpp
  Reconcile.cpp
  Layout.cpp
  Derive.cpp
  Paint.cpp
  Transitions.cpp
  Query.cpp
  Material.cpp
  ComposeInternal.h
  ComposeRuntime.h
  include/sigilcompose/Brushes.h
  include/sigilcompose/Compose.h
  include/sigilcompose/Console.h
  include/sigilcompose/Instances.h
  include/sigilcompose/Kinetic.h
  include/sigilcompose/LayerStyles.h
  include/sigilcompose/Material.h
  include/sigilcompose/Pattern.h
  include/sigilcompose/Patterns.h
  include/sigilcompose/Sdf.h
  include/sigilcompose/Decorations.h
  include/sigilcompose/Layouts.h
  include/sigilcompose/Routers.h
  include/sigilcompose/Shapes.h
  include/sigilcompose/Util.h
  include/sigilcompose/Web.h
)

target_compile_features(SigilCompose PUBLIC cxx_std_20)

target_include_directories(SigilCompose PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilCompose
  PUBLIC
    SigilImage
    SigilMotion
    SigilWeave
    unofficial::skia::skia
  PRIVATE
    yoga::yogacore
)

# OpenColorIO output view transforms (<sigilcompose/Ocio.h>): OCIO is already
# in the dependency closure (transitively via OpenImageIO), so this adds no
# new vcpkg dependency — the extension simply disables with a warning if the
# package is absent (the SPELLCIRCLE_ENABLE_ULTRALIGHT pattern).
find_package(OpenColorIO CONFIG QUIET)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilCompose PRIVATE Ocio.cpp include/sigilcompose/Ocio.h)
  target_link_libraries(SigilCompose PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilCompose PUBLIC SIGILCOMPOSE_ENABLE_OCIO)
else()
  message(WARNING "OpenColorIO not found — <sigilcompose/Ocio.h> disabled")
endif()

add_executable(compose_test
  test/ComposeTest.cpp
)
target_link_libraries(compose_test PRIVATE
  SigilCompose SigilWeavePorts GTest::gtest_main)
add_test(NAME compose_test COMMAND compose_test)

# GPU-backend behavior tests (Graphite Metal): the raster suite proves
# semantics, this one proves the same pixels arrive on the GPU — it
# exists for the direct-image draw gaps (lattice/atlas) the raster suite
# cannot see.
if(APPLE AND TARGET SpellCircleSkia)
  add_executable(compose_gpu_test
    test/ComposeGpuTest.mm
  )
  target_link_libraries(compose_gpu_test PRIVATE
    SigilCompose SigilWeavePorts SpellCircleSkia GTest::gtest_main
    "-framework Metal")
  add_test(NAME compose_gpu_test COMMAND compose_gpu_test)
endif()

# SigilScry integration (the web() leaf, stress item 19) — the kernel
# never links SigilScry; the adapter is header-only and this dedicated
# test target exists only when the Ultralight SDK produced the target.
if(TARGET SigilScry)
  add_executable(compose_web_test
    test/ComposeWebTest.cpp
  )
  target_link_libraries(compose_web_test PRIVATE
    SigilCompose SigilScry SigilWeavePorts GTest::gtest_main)
  ultralight_copy_resources(compose_web_test)
  add_test(NAME compose_web_test COMMAND compose_web_test)
endif()

# The pre-implementation feasibility spike stays: it pins the
# Yoga+SigilWeave measurement contract independently of the kernel.
add_executable(compose_spike_test
  test/ComposeSpikeTest.cpp
)
target_link_libraries(compose_spike_test PRIVATE
  SigilWeave SigilWeavePorts yoga::yogacore GTest::gtest_main)
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
  SigilCompose SigilWeavePorts benchmark::benchmark EnTT::EnTT)
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
target_link_libraries(compose_demo PRIVATE SigilCompose SigilWeavePorts)

# Each tree-structured product owns its CMakeLists (repo convention —
# see sigilweave/examples/*).
add_subdirectory(gallery)
add_subdirectory(sketch)
