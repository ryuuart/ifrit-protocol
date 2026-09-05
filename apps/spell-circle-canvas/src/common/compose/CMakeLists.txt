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
#   SigilComposeVideo       video/       a streaming SigilVideo leaf sampled
#                                        by the composer's motion clock
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
#   SigilCompose                         the whole-library name: Kit, Brush
#                                        and Typography, which reach Core
#
# Every public header lives under one include root, include/sigilcompose/
# <feature>/ — except the testing feature's read-back checks, which sit
# under a root of their own (testing/include/) so that only a target that
# links SigilComposeTesting can reach them; <sigilcompose/Compose.h> at
# the root is the umbrella over the kernel — exactly core/Core.h — and
# each feature has a <Feature>.h umbrella of its own.
#
# The tests are one binary per feature target, each with a support header
# under test/support that includes only the headers its target exposes,
# over the shared harness in test/support/Host.h. Suites interleave across
# the files rather than each file holding one suite, and a file named
# `<Source><Tier>` holds the tier's share of a source file whose suites
# straddle tiers — so search for a test by name rather than guessing which
# file owns it. Every test binary links exactly the targets it names plus
# SigilComposeTesting and the system font port, so a test reaching past
# its tier fails to link rather than passing on a dependency it was not
# meant to have. The benchmarks are one binary per feature over
# bench/BenchSupport.h, and the ones that ask for the Graphite arm get it
# where a GPU device can be made.
#
# Committed test assets sit in test/assets/: two generated variable
# fonts, an advance-variant wght instrument and a vertical-advance one.
# Not fetch_assets — a TEST must never depend on an opt-in target.
sigil_library_root(SigilCompose
  BRIEF "Immutable value-typed descriptions of a 2D scene, laid out with flexbox and drawn to an SkCanvas"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md)

# Dependencies point down this list. testing/ precedes the features whose
# test binaries link it; kit/ links the tiers above it.
add_subdirectory(core)
add_subdirectory(testing)
add_subdirectory(typography)
add_subdirectory(brush)
add_subdirectory(texture)
add_subdirectory(video)
add_subdirectory(web)
add_subdirectory(draw)
add_subdirectory(kit)

# ---------------------------------------------------------------------------
# SigilCompose — the whole-library name, the way SigilWeave, SigilMotion
# and SigilGeometry each keep one. It carries no code of its own; linking
# it is exactly linking Kit, Brush and Typography, which between them
# reach Core. The sketch library links it, for the sketch API surface;
# every test and bench in this library names the feature targets it draws
# with instead, which is what keeps a dependency on a tier a stated fact
# rather than an accident of the umbrella. The web leaf is deliberately
# NOT here: it is linked by the consumers that browse with it.
sigil_library(SigilCompose
  INTERFACE SigilComposeKit SigilComposeBrush SigilComposeTypography)

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
          --include ${SIGIL_INCLUDE_DIR}/sigilcompose
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

sigil_test(compose_docs_test
  SOURCES ${COMPOSE_API_DOC_PROBES} test/ComposeTestEngineDocs.cpp
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
            SigilComposeTypography SigilComposeVideo SigilComposeDraw
            SigilComposeTesting SigilWeavePorts)

# Every public header compiles first and alone. Compose.h says each
# header stands on its own; this is what makes that a build fact rather
# than a claim — a header that leans on an earlier include's transitive
# reach breaks here, not in the next consumer's include order. Over the
# same glob the docs probe reads, against every feature target a header
# can belong to: the docs test's set, the texture feature, and the web
# leaf where the SDK produced it (its header names SigilScry, so without
# the target it is left out rather than failed).
set(COMPOSE_SELF_TEST_HEADERS ${COMPOSE_PUBLIC_HEADERS})
set(COMPOSE_SELF_TEST_LIBRARIES
    SigilComposeCore SigilComposeKit SigilComposeBrush SigilComposeTypography
    SigilComposeVideo SigilComposeDraw SigilComposeTexture)
if(TARGET SigilComposeWeb)
  list(APPEND COMPOSE_SELF_TEST_LIBRARIES SigilComposeWeb)
else()
  list(FILTER COMPOSE_SELF_TEST_HEADERS EXCLUDE REGEX "/sigilcompose/web/")
endif()
sigil_header_self_test(compose_header_self_test
  HEADERS ${COMPOSE_SELF_TEST_HEADERS}
  LIBRARIES ${COMPOSE_SELF_TEST_LIBRARIES})

# GPU-backend behavior tests (Graphite on a GPU device): the raster suites prove
# semantics, this one proves the same pixels arrive on the GPU — it
# exists for the direct-image draw gaps (lattice/atlas) the raster suites
# cannot see.
if(APPLE)
  sigil_test(compose_gpu_test
    SOURCES test/ComposeGpuTest.mm
    LIBRARIES SigilComposeKit SigilComposeBrush SigilComposeTypography
              SigilWeavePorts SigilSkia)
endif()
