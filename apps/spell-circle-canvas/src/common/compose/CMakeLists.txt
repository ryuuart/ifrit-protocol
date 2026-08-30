# SigilCompose — data-driven, cacheable, animated drawable components
# over Yoga + SigilWeave + SigilMotion + Skia. README.md is the canon.
#
# The library is a set of feature targets over one kernel. Each is its own
# archive so a consumer links what it draws with and nothing above it:
#
#   SigilComposeCore        the kernel — element values, the reconciler,
#                           layout, paint, transitions, text
#   SigilComposeShape       silhouettes, layouts and routers over Core
#   SigilComposeBrush       decorations, lines and brushes over Shape
#   SigilComposePaint       patterns, SDF materials, layer styles and OCIO
#                           over Brush
#   SigilComposeTypography  type styles and the text-fx presets (headers)
#   SigilComposeWeb         the Ultralight leaf (header; only with SigilScry)
#   SigilCompose            the umbrella: every tier above at once
#
# The kit (kit/CMakeLists.txt) and the testing checks (testing/) are their
# own libraries over these public surfaces.

# ---------------------------------------------------------------------------
# SigilComposeCore — the kernel.
#
# Holds the element value and its authoring grammar, the keyed reconciler,
# Yoga layout with SigilWeave-measured text, stacking paint with automatic
# picture caching of provably-static subtrees, transitions and bindings, the
# per-glyph text seam, the composer's queries, the material value, the
# streaming feed and the instanced leaf. Links SigilGeometryPath, SigilImage,
# SigilMotion, SigilWeave and Skia publicly, because those types appear in
# its headers; Yoga is private, because no header spells it.
#
# This is the ONLY target whose translation units see the source root —
# ComposeInternal.h and ComposeRuntime.h — and that include path is PRIVATE.
# Every other target in this tree, including the tiers below, compiles
# against the public headers alone.
add_library(SigilComposeCore STATIC
  Compose.cpp
  Composer.cpp
  Reconcile.cpp
  Layout.cpp
  Derive.cpp
  Paint.cpp
  Transitions.cpp
  TextFx.cpp
  TextAtRest.cpp
  Query.cpp
  Material.cpp
  Instances.cpp
  ComposeInternal.h
  ComposeRuntime.h
  include/sigilcompose/Compose.h
  include/sigilcompose/Composer.h
  include/sigilcompose/Derive.h
  include/sigilcompose/Element.h
  include/sigilcompose/Env.h
  include/sigilcompose/Feed.h
  include/sigilcompose/GpuImage.h
  include/sigilcompose/Instances.h
  include/sigilcompose/Layout.h
  include/sigilcompose/Mask.h
  include/sigilcompose/Material.h
  include/sigilcompose/Motion.h
  include/sigilcompose/Paint.h
  include/sigilcompose/Shape.h
  include/sigilcompose/Stroke.h
  include/sigilcompose/Text.h
)
target_compile_features(SigilComposeCore PUBLIC cxx_std_20)
target_include_directories(SigilComposeCore
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(SigilComposeCore
  PUBLIC
    SigilGeometryPath
    SigilImage
    SigilMotion
    SigilWeave
    unofficial::skia::skia
  PRIVATE
    yoga::yogacore
)

# ---------------------------------------------------------------------------
# SigilComposeShape — silhouettes and placement.
#
# The stock silhouette catalog (`shapes::`), the layout catalog
# (`layouts::`) and the routers that place children along a path. Links Core
# publicly: every value here is an Element or a Shape.
add_library(SigilComposeShape STATIC
  Shapes.cpp
  Routers.cpp
  include/sigilcompose/Layouts.h
  include/sigilcompose/Routers.h
  include/sigilcompose/Shapes.h
)
target_compile_features(SigilComposeShape PUBLIC cxx_std_20)
target_link_libraries(SigilComposeShape PUBLIC SigilComposeCore)

# ---------------------------------------------------------------------------
# SigilComposeBrush — what draws along an outline.
#
# Decorations (strokes, shadows, washes, contour walks), the line vocabulary
# (`lines::`) and the brush engine (`brush::`), with the kit's stroke grammar
# (`kit/Strokes.h`) and the bordered feed plate (`kit/Plate.h`) that are
# spelled in this tier's types. Links Shape publicly: a brush is applied to a
# silhouette.
add_library(SigilComposeBrush STATIC
  Decorations.cpp
  Lines.cpp
  Brushes.cpp
  include/sigilcompose/Brushes.h
  include/sigilcompose/Decorations.h
  include/sigilcompose/Lines.h
  include/sigilcompose/kit/Plate.h
  include/sigilcompose/kit/Strokes.h
)
target_compile_features(SigilComposeBrush PUBLIC cxx_std_20)
target_link_libraries(SigilComposeBrush PUBLIC SigilComposeShape
                                              SigilComposeTypography)

# ---------------------------------------------------------------------------
# SigilComposePaint — what fills a region.
#
# Pattern tiles and the stock generators (`patterns::`), SDF materials
# (`sdf::`), layer-style presets (`styles::`) and — when OpenColorIO is in
# the dependency closure — output view transforms (<sigilcompose/Ocio.h>).
# Links Brush publicly: the layer-style presets are spelled in keylines and
# shadows.
add_library(SigilComposePaint STATIC
  Patterns.cpp
  Sdf.cpp
  LayerStyles.cpp
  include/sigilcompose/LayerStyles.h
  include/sigilcompose/Pattern.h
  include/sigilcompose/Patterns.h
  include/sigilcompose/Sdf.h
)
target_compile_features(SigilComposePaint PUBLIC cxx_std_20)
target_link_libraries(SigilComposePaint PUBLIC SigilComposeBrush)

# OpenColorIO is already in the dependency closure (transitively via
# OpenImageIO), so this adds no new vcpkg dependency — the extension simply
# disables with a warning if the package is absent. The definition is PUBLIC
# so a consumer can `#ifdef SIGILCOMPOSE_ENABLE_OCIO` around its use.
find_package(OpenColorIO CONFIG QUIET)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilComposePaint PRIVATE Ocio.cpp include/sigilcompose/Ocio.h)
  target_link_libraries(SigilComposePaint PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilComposePaint PUBLIC SIGILCOMPOSE_ENABLE_OCIO)
else()
  message(WARNING "OpenColorIO not found — <sigilcompose/Ocio.h> disabled")
endif()

# ---------------------------------------------------------------------------
# SigilComposeTypography — type styles and text-fx presets.
#
# Header-only: `type()` and `pickFace()` (Typography.h), the stock text
# effects and the marquee (TextFx.h), and the kit's legibility helpers
# (kit/Legibility.h). Everything here is a value built from Core's public
# types, so the target carries no archive and links Core as an interface.
add_library(SigilComposeTypography INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include/sigilcompose/TextFx.h
  ${CMAKE_CURRENT_LIST_DIR}/include/sigilcompose/Typography.h
  ${CMAKE_CURRENT_LIST_DIR}/include/sigilcompose/kit/Legibility.h
)
target_link_libraries(SigilComposeTypography INTERFACE SigilComposeCore)

# ---------------------------------------------------------------------------
# SigilComposeWeb — the live page as a leaf.
#
# Header-only adapter from a SigilScry web view to a custom leaf (Web.h).
# Defined only when the Ultralight SDK produced SigilScry, so that nothing
# in this tree links a browser engine by accident: a consumer that wants the
# leaf names this target and gets SigilScry with it.
if(TARGET SigilScry)
  add_library(SigilComposeWeb INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include/sigilcompose/Web.h
  )
  target_link_libraries(SigilComposeWeb INTERFACE SigilComposeCore SigilScry)
endif()

# ---------------------------------------------------------------------------
# SigilCompose — the umbrella.
#
# Every tier at once, for a consumer that draws with all of it — the gallery,
# the sketch host, the products. It carries no code of its own; linking it
# is exactly linking Paint (which reaches Brush, Shape and Core) plus
# Typography. The web leaf is deliberately NOT here, because it would make
# every umbrella consumer link a browser engine.
add_library(SigilCompose INTERFACE)
target_link_libraries(SigilCompose INTERFACE
  SigilComposePaint SigilComposeTypography)

# The tests are one binary per library target, each with a support header
# under test/support that includes only the headers its target exposes,
# over the shared harness in test/support/Host.h. Editing a catalog header
# therefore rebuilds the binaries that exercise it and no other. Suites
# interleave across the files rather than each file holding one suite, and
# a file named `<Source><Tier>` holds the tier's share of a source file
# whose suites straddle tiers — so search for a test by name rather than
# guessing which file owns it.
#
# `_compose_test_binary(<name> LIBRARIES <targets>... SOURCES <files>...)`
# registers one binary over exactly the targets it names — nothing links
# the umbrella but the docs probes, so a test reaching past its tier fails
# to link rather than passing on a dependency it was not meant to have.
function(_compose_test_binary name)
  cmake_parse_arguments(ARG "" "" "LIBRARIES;SOURCES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE
    ${ARG_LIBRARIES} SigilComposeTesting SigilWeavePorts GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# The kernel: elements, the reconciler, layout, paint, transitions, text,
# the feed, the instanced leaf, masks and the field walks. Links Core ALONE,
# which is the proof that the kernel stands without its catalogs. The field
# walks include ComposeInternal.h AND ComposeRuntime.h (the only test TU
# that does either) so the equality comparators and the kSlotSpecs table
# can be reached DIRECTLY — see the file header — which is why this binary
# alone sees the library's source root. yoga::yogacore is PRIVATE to Core,
# so its headers are not on this target's path by inheritance — and
# ComposeRuntime.h includes <yoga/Yoga.h>. Listed for the INCLUDE PATH; the
# archive itself was already in the link closure.
_compose_test_binary(compose_core_test
  LIBRARIES SigilComposeCore
  SOURCES
    test/ComposeTestKernel.cpp
    test/ComposeTestMask.cpp
    test/ComposeTestFieldPins.cpp
    test/ComposeTestContentCore.cpp
    test/ComposeTestEngineDocsCore.cpp
)
target_include_directories(compose_core_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(compose_core_test PRIVATE yoga::yogacore)

# Silhouettes, layouts, routers, rails and travel along a path.
_compose_test_binary(compose_shape_test
  LIBRARIES SigilComposeShape
  SOURCES
    test/ComposeTestContentShape.cpp
    test/ComposeTestKernelShape.cpp
    test/ComposeTestLinesShape.cpp
)

# Decorations, lines, rails, hatches, brushes, the stroke grammar (which is
# what the authoring-grammar file exercises), the kit's stroke presets, the
# brushed masks and the stamped and stroked slices of the other catalogs.
_compose_test_binary(compose_brush_test
  LIBRARIES SigilComposeBrush
  SOURCES
    test/ComposeTestLines.cpp
    test/ComposeTestBrushes.cpp
    test/ComposeTestGrammar.cpp
    test/ComposeTestContentBrush.cpp
    test/ComposeTestEngineDocsBrush.cpp
    test/ComposeTestKernelBrush.cpp
    test/ComposeTestMaskBrush.cpp
    test/ComposeTestTextDataBrush.cpp
    test/ComposeTestKitBrush.cpp
)

# Patterns, SDF materials, layer styles, the pattern pan and colour
# management.
_compose_test_binary(compose_paint_test
  LIBRARIES SigilComposePaint
  SOURCES
    test/ComposeTestBrushesPaint.cpp
    test/ComposeTestContentPaint.cpp
    test/ComposeTestEngineDocsPaint.cpp
    test/ComposeTestGrammarPaint.cpp
    test/ComposeTestKernelPaint.cpp
    test/ComposeTestLinesPaint.cpp
    test/ComposeTestMaskPaint.cpp
    test/ComposeTestTextDataPaint.cpp
)

# Text data, the text pass, vertical writing, motion along paths, the
# text-fx presets, rich spans and the variation drive. Typography is
# header-only over Core; Shape is here because a path baseline and an
# upright column are exercised on `shapes::` silhouettes.
_compose_test_binary(compose_text_test
  LIBRARIES SigilComposeTypography SigilComposeShape
  SOURCES
    test/ComposeTestTextData.cpp
    test/ComposeTestTextPass.cpp
    test/ComposeTestVertical.cpp
    test/ComposeTestPathMotion.cpp
    test/ComposeTestContentText.cpp
    test/ComposeTestEngineDocsText.cpp
)

# Composer queries, the studio, the feed plate and the instruments that
# read a cascade back.
_compose_test_binary(compose_studio_test
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
  SOURCES
    test/ComposeTestQueriesStudio.cpp
    test/ComposeTestContentStudio.cpp
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
          --include ${CMAKE_CURRENT_SOURCE_DIR}/testing/include/sigilcompose
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
  LIBRARIES SigilCompose SigilComposeKit
  SOURCES
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
foreach(_t compose_text_test compose_docs_test)
  target_compile_definitions(${_t} PRIVATE
    SIGILCOMPOSE_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/assets")
endforeach()

# The kit tier (include/sigilcompose/kit/*.h). Separate binary rather than
# more cases in the kernel binaries: the kit changes nothing in the kernel,
# so a kit failure must never look like a kernel failure on the dashboard.
# Links the kit ALONE — the stroke-grammar cases, which need the Brush
# tier, are the brush binary's.
add_executable(compose_kit_test
  test/ComposeKitTest.cpp
)
target_link_libraries(compose_kit_test PRIVATE
  SigilComposeKit SigilComposeTesting SigilWeavePorts GTest::gtest_main)
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
    SigilComposeWeb SigilWeavePorts GTest::gtest_main)
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

# ---------------------------------------------------------------------------
# Benchmarks (Google Benchmark; build in Release before reading anything
# from them), one binary per tier so each links only the library it
# measures. None is a test: they build through the `benches` target and run
# through scripts/bench_ledger.py, which compares them against a baseline.
#
# The Graphite arms (COMPOSE_BENCH_GRAPHITE) exist only where the Metal
# context can be made; the binaries that carry them take the bench-only
# Metal handles as an extra source.
if(APPLE AND TARGET SpellCircleSkia)
  set(COMPOSE_BENCH_GPU_SOURCES
      bench/ComposeBenchGpuMetal.mm bench/ComposeBenchGpu.h)
  set_source_files_properties(bench/ComposeBenchGpuMetal.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
else()
  set(COMPOSE_BENCH_GPU_SOURCES)
endif()

# `_compose_bench(<name> LIBRARIES <targets>... [GPU] SOURCES <files>...)`
# registers one benchmark binary over bench/BenchSupport.h. GPU adds the
# Graphite arms when the platform can build them.
function(_compose_bench name)
  cmake_parse_arguments(ARG "GPU" "" "LIBRARIES;SOURCES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES} bench/BenchSupport.h)
  target_link_libraries(${name} PRIVATE
    ${ARG_LIBRARIES} SigilWeavePorts benchmark::benchmark)
  if(ARG_GPU AND APPLE AND TARGET SpellCircleSkia)
    target_sources(${name} PRIVATE ${COMPOSE_BENCH_GPU_SOURCES})
    target_compile_definitions(${name} PRIVATE COMPOSE_BENCH_GRAPHITE)
    target_link_libraries(${name} PRIVATE SpellCircleSkia ${METAL_FRAMEWORK})
  endif()
  add_dependencies(benches ${name})
endfunction()

# The kernel: describe, reconcile, layout, query, caching, and the
# whole-scene scoreboard, blur, instances and particle arms.
_compose_bench(compose_core_bench GPU
  LIBRARIES SigilComposeCore EnTT::EnTT
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreSceneBench.cpp)
# Comparable silhouettes, hit testing and blended shaped leaves.
_compose_bench(compose_shape_bench
  LIBRARIES SigilComposeShape
  SOURCES bench/ComposeShapeBench.cpp)
# Masks, brushes, profiled and span strokes, stamps, warps, hatches and
# the binding cost under stroked cells.
_compose_bench(compose_brush_bench
  LIBRARIES SigilComposeBrush
  SOURCES bench/ComposeBrushBench.cpp)
# Pattern bakes and fills, SDF materials against paths, layer-style shadows.
_compose_bench(compose_paint_bench
  LIBRARIES SigilComposePaint
  SOURCES bench/ComposePaintBench.cpp)
# Dense text replayed, baked and projected; kinetic text every frame.
_compose_bench(compose_text_bench GPU
  LIBRARIES SigilComposeTypography
  SOURCES bench/ComposeTextBench.cpp)

# Headless gallery: renders stress-catalog panels to PNG (out dir:
# first argument, default ./compose_demo_out). The interactive Qt
# ComposeGallery follows in a later phase.
add_executable(compose_demo
  examples/compose_demo.cpp
)
target_link_libraries(compose_demo PRIVATE SigilCompose SigilWeavePorts)

# Each tree-structured product owns its CMakeLists (repo convention —
# see sigilweave/examples/*). testing/ and kit/ first (they are libraries
# the rest may link), then sketch/, which defines SigilSketchStudies (every
# study sketch compiled in place) and which the gallery links.
add_subdirectory(testing)
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
