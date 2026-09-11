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
# One test binary, compose_test, declared feature by feature from each
# directory's own test/, and one bench binary, compose_bench, the same
# way. What locates a case is its name, not a binary: ctest discovers one
# entry per CASE, so a suite is selected by its own prefix and nothing
# stands behind it to build. Each declaration links exactly the targets
# its cases name plus SigilComposeTesting and the system font port, so a
# case reaching past its feature fails to link rather than passing on a
# dependency it was not meant to have.
#
# One file per subject, named for what it asserts, so a case is found by
# opening the file its subject names. Each feature has a support header
# under test/support that includes only the headers its targets expose,
# over the shared harness in test/support/Host.h; the font context that
# harness holds is the tree-wide src/test/Fonts.h. A case that skips or
# vanishes without a device, a font or an SDK carries the ctest label
# that says so, scoped by SUITES to the cases that need it.
#
# The benchmarks stand over bench/BenchSupport.h, and the ones that ask
# for the Graphite arm get it where a GPU device can be made. A claim
# about how a cost GROWS lives there rather than in a ctest wall-clock
# ceiling.
#
# Committed test assets sit in test/assets/: one generated variable font,
# the vertical-advance instrument, beside the script that generates it.
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
# canon as the page that links it. The testing tier's own include root is
# scanned beside the library's, since the walkthroughs spell the harness
# the cases are written against.
sigil_doc_probes(compose
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/testing/include/sigilcompose
           ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../measure/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/kit
           ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/path
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/skia
           ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include/sigilskia/draw
           ${CMAKE_CURRENT_SOURCE_DIR}/../draw/include/sigildraw
           ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../test
  PRELUDES "\"support/DocsTestSupport.h\""
           "<sigilskia/draw/Direct.h>"
  ALIASES ch=choreograph
  SKIP_HEADERS Web.h
  FLOORS 40,3,12,40,1
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
            SigilComposeTypography SigilComposeVideo SigilComposeDraw
            SigilComposeTesting SigilWeavePorts SigilSkiaDraw)

sigil_test(compose_test
  SOURCES test/ComposeTestDocs.cpp
  LIBRARIES compose_api_doc_probes SigilComposeCore SigilComposeKit
            SigilComposeBrush SigilComposeTypography SigilComposeVideo
            SigilComposeDraw SigilComposeTesting SigilWeavePorts)

# Every public header compiles first and alone. Compose.h says each
# header stands on its own; this is what makes that a build fact rather
# than a claim — a header that leans on an earlier include's transitive
# reach breaks here, not in the next consumer's include order. Over the
# same glob the docs probe reads, against every feature target a header
# can belong to: the docs test's set, the texture feature, and the web
# leaf where the SDK produced it (its header names SigilScry, so without
# the target it is left out rather than failed).
file(GLOB_RECURSE COMPOSE_SELF_TEST_HEADERS
     CONFIGURE_DEPENDS include/sigilcompose/*.h)
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
  sigil_test(compose_test
    SOURCES test/ComposeGpuTest.mm
    LIBRARIES SigilComposeKit SigilComposeBrush SigilComposeTypography
              SigilWeavePorts SigilSkia
    SUITES ComposeGpu DirectImageDraw LABELS gpu)
endif()
