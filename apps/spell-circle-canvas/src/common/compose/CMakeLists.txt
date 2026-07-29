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
  include/sigilcompose/Debug.h
  include/sigilcompose/Sdf.h
  include/sigilcompose/Decorations.h
  include/sigilcompose/Layouts.h
  include/sigilcompose/Routers.h
  include/sigilcompose/Shapes.h
  include/sigilcompose/Studio.h
  include/sigilcompose/Util.h
  include/sigilcompose/Web.h
)
# The KIT tier is NOT listed here: it is its own library (kit/CMakeLists.txt)
# whose include path is compose's public headers alone, which is what makes
# the tier boundary structural instead of conventional.

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

# Nine contiguous slices of what was one 12.6k-line ComposeTest.cpp —
# byte-identical test bodies in original order, sharing
# test/ComposeTestSupport.h (the Host harness and helpers). Named by each
# slice's dominant theme; suites interleave across slices because the
# original file grew chronologically, and reshuffling 440 tests by suite
# was judged a bigger risk than a name that is only mostly right.
# The docs' documented names, compiled against the headers that own them.
# ROADMAP §25: the hand-transcribed `ComposeDocs.EverySignature…` guards
# cover the sections someone remembered to transcribe, which is how a
# `PathFormat{.effects = …}` that does not compile reached page one. This
# TU is GENERATED from the docs on every build, so a new section is covered
# the moment it is written and a rename that the prose misses is a build
# break rather than a confident wrong answer. The generator FAILS on a
# documented name no header declares — the guard cannot go quiet.
set(COMPOSE_API_DOC_PROBES
    ${CMAKE_CURRENT_BINARY_DIR}/ComposeApiDocProbes.cpp)
file(GLOB_RECURSE COMPOSE_PUBLIC_HEADERS
     CONFIGURE_DEPENDS include/sigilcompose/*.h)
add_custom_command(
  OUTPUT ${COMPOSE_API_DOC_PROBES}
  COMMAND ${CMAKE_COMMAND} -E env python3
          ${CMAKE_CURRENT_SOURCE_DIR}/test/docs/api_doc_probes.py
          --md ${CMAKE_CURRENT_SOURCE_DIR}/API.md
          --md ${CMAKE_CURRENT_SOURCE_DIR}/DESIGN.md
          --md ${CMAKE_CURRENT_SOURCE_DIR}/STRESS_TESTS.md
          --include ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilcompose
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
          --out ${COMPOSE_API_DOC_PROBES}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/API.md
          ${CMAKE_CURRENT_SOURCE_DIR}/DESIGN.md
          ${CMAKE_CURRENT_SOURCE_DIR}/STRESS_TESTS.md
          ${CMAKE_CURRENT_SOURCE_DIR}/test/docs/api_doc_probes.py
          ${COMPOSE_PUBLIC_HEADERS}
  COMMENT "Extracting the compose docs' documented names into compile probes"
  VERBATIM)

add_executable(compose_test
  ${COMPOSE_API_DOC_PROBES}
  test/ComposeTestKernel.cpp
  test/ComposeTestContent.cpp
  test/ComposeTestLines.cpp
  test/ComposeTestBrushes.cpp
  test/ComposeTestEngineDocs.cpp
  test/ComposeTestTextData.cpp
  test/ComposeTestQueriesStudio.cpp
  test/ComposeTestGrammar.cpp
  test/ComposeTestMask.cpp
)
target_link_libraries(compose_test PRIVATE
  SigilCompose SigilWeavePorts GTest::gtest_main)
# The generated probe TU lives in the binary dir and includes the shared
# harness by name, like every hand-written slice does.
target_include_directories(compose_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
add_test(NAME compose_test COMMAND compose_test)

# The kit tier (include/sigilcompose/kit/*.h). Separate binary rather than
# more cases in compose_test: the kit changes nothing in the kernel, so a
# kit failure must never look like a kernel failure on the dashboard.
add_executable(compose_kit_test
  test/ComposeKitTest.cpp
)
target_link_libraries(compose_kit_test PRIVATE
  SigilComposeKit SigilWeavePorts GTest::gtest_main)
add_test(NAME compose_kit_test COMMAND compose_kit_test)

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
  bench/ComposeCoreBench.cpp
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
# see sigilweave/examples/*). kit/ first (it is a library the rest may
# link), then sketch/, which defines SigilSketchStudies (every study sketch
# compiled in place) and which the gallery links.
add_subdirectory(kit)
add_subdirectory(sketch)
add_subdirectory(gallery)
