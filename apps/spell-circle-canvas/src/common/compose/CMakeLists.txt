# SigilCompose — data-driven, cacheable, animated drawable components
# over Yoga + SigilWeave + SigilMotion + Skia, as one feature library per
# directory. README.md is the canon.
#
# Each feature is its own target with its own tests and benchmarks, and
# links only the features beneath it, so a consumer links what it draws
# with and nothing above it:
#
#   SigilComposeCore        core/        the kernel — element values, the
#                                        reconciler, layout, paint,
#                                        transitions, text at rest, the
#                                        instanced sprite leaf
#   SigilComposeTypography  typography/  the text engine (fx tracks, path
#                                        text, marks), type styles and the
#                                        text-fx presets
#   SigilComposeBrush       brush/       decorations, lines, brushes, layer
#                                        styles, the stroke grammar's engine
#                                        and the mask gates
#   SigilComposeTesting     testing/     the read-back checks (headers)
#   SigilComposeTexture     texture/     a scene painted into a surface and
#                                        handed over as a material texture
#   SigilComposeWeb         web/         the Ultralight leaf (headers; only
#                                        with SigilScry)
#   SigilComposeDraw        draw/        the door to SigilDraw's pen, both
#                                        ways: a node running a pen program,
#                                        and an element retained in a pen
#   SigilComposeKit         kit/         the kit tier: the silhouette shelf
#                                        spelled for a node, the layout
#                                        schemes, the routers, the placers,
#                                        the kinetic type presets and the
#                                        instruments over Typography
#   SigilCompose                         the whole-library convenience for
#                                        an OUTSIDE consumer: Kit, Brush and
#                                        Typography, which reach Core;
#                                        nothing in this tree links it
#
# Every public header lives under one include root, include/sigilcompose/
# <feature>/, so a feature adds the root and spells its own directory —
# except the testing feature's read-back checks, which sit under a root of
# their own (testing/include/) so that only a target that links
# SigilComposeTesting can reach them;
# <sigilcompose/Compose.h> at the root is the transitional umbrella over
# the kernel and each feature has a <Feature>.h umbrella of its own. Test
# fixtures every binary shares sit in test/support/ and the bench support
# in bench/; a feature's test and bench binaries add those directories.

set(SIGIL_COMPOSE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SIGIL_COMPOSE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test)
set(SIGIL_COMPOSE_BENCH_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/bench)
# Committed test assets (the SigilImage test/assets convention): today one
# generated variable font, an advance-variant wght instrument. Not
# fetch_assets — a TEST must never depend on an opt-in target.
set(SIGIL_COMPOSE_TEST_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/test/assets)

# The tests are one binary per feature target, each with a support header
# under test/support that includes only the headers its target exposes,
# over the shared harness in test/support/Host.h. Suites interleave across
# the files rather than each file holding one suite, and a file named
# `<Source><Tier>` holds the tier's share of a source file whose suites
# straddle tiers — so search for a test by name rather than guessing which
# file owns it.
#
# `_compose_test_binary(<name> LIBRARIES <targets>... SOURCES <files>...)`
# registers one binary over exactly the targets it names — nothing links
# the umbrella, so a test reaching past its tier fails to link rather than
# passing on a dependency it was not meant to have.
function(_compose_test_binary name)
  cmake_parse_arguments(ARG "" "" "LIBRARIES;SOURCES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  target_include_directories(${name} PRIVATE ${SIGIL_COMPOSE_TEST_SUPPORT_DIR})
  target_link_libraries(${name} PRIVATE
    ${ARG_LIBRARIES} SigilComposeTesting SigilWeavePorts GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# Benchmarks (Google Benchmark; build in Release before reading anything
# from them), one binary per feature so each links only the library it
# measures. None is a test: they build through the `benches` target and run
# through scripts/bench_ledger.py, which compares them against a baseline.
#
# The Graphite arms (COMPOSE_BENCH_GRAPHITE) exist only where a GPU device
# can be made; they reach it through SigilSkia and name no graphics API.

# `_compose_bench(<name> LIBRARIES <targets>... [GPU] SOURCES <files>...)`
# registers one benchmark binary over bench/BenchSupport.h. GPU adds the
# Graphite arms when the platform can build them.
function(_compose_bench name)
  cmake_parse_arguments(ARG "GPU" "" "LIBRARIES;SOURCES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES}
    ${SIGIL_COMPOSE_BENCH_SUPPORT_DIR}/BenchSupport.h)
  target_include_directories(${name} PRIVATE ${SIGIL_COMPOSE_BENCH_SUPPORT_DIR})
  target_link_libraries(${name} PRIVATE
    ${ARG_LIBRARIES} SigilWeavePorts benchmark::benchmark)
  if(ARG_GPU AND APPLE AND TARGET SigilSkia)
    target_compile_definitions(${name} PRIVATE COMPOSE_BENCH_GRAPHITE)
    target_link_libraries(${name} PRIVATE SigilSkia)
  endif()
  add_dependencies(benches ${name})
endfunction()

# Dependencies point down this list. testing/ precedes the features whose
# test binaries link it; kit/ links the tiers above it.
add_subdirectory(core)
add_subdirectory(testing)
add_subdirectory(typography)
add_subdirectory(brush)
add_subdirectory(texture)
add_subdirectory(web)
add_subdirectory(draw)
add_subdirectory(kit)

# ---------------------------------------------------------------------------
# SigilCompose — the whole-library name, kept as a convenience for a
# consumer OUTSIDE this tree, the way SigilWeave, SigilMotion and
# SigilGeometry each keep one. It carries no code of its own; linking it is
# exactly linking Kit, Brush and Typography, which between them reach Core.
# Nothing in this repository links it: every consumer here —
# the benches, the demos, the tests, and the sketch library above — names
# the feature targets it draws with, which is what keeps a dependency on a
# tier a stated fact rather than an accident of the umbrella. The web leaf
# is deliberately NOT here: it is linked by the consumers that browse with
# it.
add_library(SigilCompose INTERFACE)
target_link_libraries(SigilCompose INTERFACE
  SigilComposeKit SigilComposeBrush SigilComposeTypography)

# ---------------------------------------------------------------------------
# The cross-feature tests, which are the library's rather than one tier's.

# The engine walkthroughs and the compile check over the prose.
# Every API name README.md and TYPOGRAPHY.md spell, compiled against the
# headers that own it — both files, because the type chapter is the same
# canon as the page that links it and prose nobody compiles is prose that
# goes stale. A hand-written guard only covers the sections someone remembered to
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
          --md ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md
          --include ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose
          --include ${CMAKE_CURRENT_SOURCE_DIR}/testing/include/sigilcompose
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../measure/include
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/kit
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/path
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/skia
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include/sigilskia/draw
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../draw/include/sigildraw
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
          --out ${COMPOSE_API_DOC_PROBES}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md
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
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
            SigilComposeTypography SigilComposeDraw
  SOURCES
    ${COMPOSE_API_DOC_PROBES}
    test/ComposeTestEngineDocs.cpp
)
target_compile_definitions(compose_docs_test PRIVATE
  SIGILCOMPOSE_TEST_ASSET_DIR="${SIGIL_COMPOSE_TEST_ASSET_DIR}")

# GPU-backend behavior tests (Graphite on a GPU device): the raster suites prove
# semantics, this one proves the same pixels arrive on the GPU — it
# exists for the direct-image draw gaps (lattice/atlas) the raster suites
# cannot see.
if(APPLE AND TARGET SigilSkia)
  add_executable(compose_gpu_test
    test/ComposeGpuTest.mm
  )
  target_include_directories(compose_gpu_test PRIVATE
    ${SIGIL_COMPOSE_TEST_SUPPORT_DIR})
  target_link_libraries(compose_gpu_test PRIVATE
    SigilComposeKit SigilComposeBrush SigilComposeTypography
    SigilWeavePorts SigilSkia
    GTest::gtest_main)
  add_test(NAME compose_gpu_test COMMAND compose_gpu_test)
endif()

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilCompose
  BRIEF "Immutable value-typed descriptions of a 2D scene, laid out with flexbox and drawn to an SkCanvas"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
