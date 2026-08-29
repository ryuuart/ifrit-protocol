# SigilCompose — data-driven, cacheable, animated drawable components
# over Yoga + SigilWeave + Choreograph/SigilMotion + Skia: element values,
# a keyed reconciler, Yoga layout with SigilWeave-measured text, stacking
# paint, automatic picture caching of provably-static subtrees, and
# Choreograph transitions/bindings. README.md is the canon.
add_library(SigilCompose STATIC
  Compose.cpp
  Composer.cpp
  Reconcile.cpp
  Layout.cpp
  Derive.cpp
  Paint.cpp
  Transitions.cpp
  TextFx.cpp
  Debug.cpp
  Query.cpp
  Material.cpp
  ComposeInternal.h
  ComposeRuntime.h
  include/sigilcompose/Brushes.h
  include/sigilcompose/Compose.h
  include/sigilcompose/Feed.h
  include/sigilcompose/Instances.h
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
  include/sigilcompose/TextFx.h
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
    SigilGeometryPath
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

# The kernel tests are one binary per feature area, each with a support
# header under test/support that includes only the headers its translation
# units use, over the shared harness in test/support/Host.h. Editing an
# extension header therefore rebuilds the binaries that exercise it and no
# other. Suites interleave across the files rather than each file holding
# one suite, so search for a test by name rather than guessing which file
# owns it.
#
# `_compose_test_binary(<name> <sources>...)` registers one such binary.
function(_compose_test_binary name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE
    SigilCompose SigilWeavePorts GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# The kernel, the authoring grammar, masks and the field walks. The field
# walks include ComposeInternal.h AND ComposeRuntime.h (the only test TU
# that does either) so the equality comparators and the kSlotSpecs table
# can be reached DIRECTLY — see the file header — which is why this binary
# alone sees the library's source root. yoga::yogacore is PRIVATE to
# SigilCompose, so its headers are not on this target's path by
# inheritance — and ComposeRuntime.h includes <yoga/Yoga.h>. Listed for
# the INCLUDE PATH; the archive itself was already in the link closure.
_compose_test_binary(compose_core_test
  test/ComposeTestKernel.cpp
  test/ComposeTestGrammar.cpp
  test/ComposeTestMask.cpp
  test/ComposeTestFieldPins.cpp
)
target_include_directories(compose_core_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(compose_core_test PRIVATE yoga::yogacore)

# Leaves, feed, routers, layouts, text effects and colour management.
_compose_test_binary(compose_content_test
  test/ComposeTestContent.cpp
)

# Lines, rails, hatches and brushes.
_compose_test_binary(compose_brush_test
  test/ComposeTestLines.cpp
  test/ComposeTestBrushes.cpp
)

# Text data, the text pass, vertical writing and motion along paths.
_compose_test_binary(compose_text_test
  test/ComposeTestTextData.cpp
  test/ComposeTestTextPass.cpp
  test/ComposeTestVertical.cpp
  test/ComposeTestPathMotion.cpp
)

# Composer queries and the studio.
_compose_test_binary(compose_studio_test
  test/ComposeTestQueriesStudio.cpp
)

# The engine walkthroughs and the README's compile check.
# Every API name the README spells, compiled against the headers that own
# it. A hand-written guard only covers the sections someone remembered to
# transcribe, which is how documentation that does not compile reaches a
# reader. This TU is GENERATED from the document on every build, so new
# prose is covered the moment it is written and a rename the prose misses
# is a build break rather than a confident wrong answer. The generator
# FAILS on a documented name no header declares — the guard cannot go
# quiet.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(COMPOSE_API_DOC_PROBES
    ${CMAKE_CURRENT_BINARY_DIR}/ComposeApiDocProbes.cpp)
file(GLOB_RECURSE COMPOSE_PUBLIC_HEADERS
     CONFIGURE_DEPENDS include/sigilcompose/*.h)
add_custom_command(
  OUTPUT ${COMPOSE_API_DOC_PROBES}
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/test/docs/api_doc_probes.py
          --md ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          --include ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilcompose
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
          --out ${COMPOSE_API_DOC_PROBES}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/test/docs/api_doc_probes.py
          ${COMPOSE_PUBLIC_HEADERS}
  COMMENT "Extracting the README's documented names into compile probes"
  VERBATIM)

# The generator's own fixtures (--self-test): a real name must yield a
# probe, an unreal one must fail the run, an operator spelling must be
# exempted AND reported by name, and an EXTERNAL_CLASSES member must take
# the class-scope probe path. One Python invocation, no C++ — and the only
# check that notices the EXTRACTOR narrowing, since a generator that
# silently probes less still emits a probe TU that compiles green.
add_test(NAME compose_api_doc_probes_self_test
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/test/docs/api_doc_probes.py
          --self-test)

_compose_test_binary(compose_docs_test
  ${COMPOSE_API_DOC_PROBES}
  test/ComposeTestEngineDocs.cpp
)
# The generated probe TU lives in the binary dir and includes its support
# header by the same path the hand-written slices use.
target_include_directories(compose_docs_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)

# Committed test assets (the SigilImage test/assets convention): today one
# generated variable font, an advance-variant wght instrument. Not
# fetch_assets — a TEST must never depend on an opt-in target. Defined for
# the binaries whose tests open it.
foreach(_t compose_content_test compose_text_test compose_docs_test)
  target_compile_definitions(${_t} PRIVATE
    SIGILCOMPOSE_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/assets")
endforeach()

# The kit tier (include/sigilcompose/kit/*.h). Separate binary rather than
# more cases in the kernel binaries: the kit changes nothing in the kernel,
# so a kit failure must never look like a kernel failure on the dashboard.
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

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilCompose
  BRIEF "Immutable value-typed descriptions of a 2D scene, laid out with flexbox and drawn to an SkCanvas"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
