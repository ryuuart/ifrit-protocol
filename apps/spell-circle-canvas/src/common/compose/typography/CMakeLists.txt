# SigilComposeTypography — the text vocabulary and the text engine.
#
# The VOCABULARY is the public headers: one unit as the layout placed it,
# the two selector forms whose subject is a description of this library,
# the effect and the effects the runtime evaluates by structure, the track
# and the beat, the reading beside the type, and the path a baseline can
# be. The text's own words — the content, the granularity, the selector
# value and every form that names a position in the text — are SigilWeave's
# and are included from there. The kernel's text leaf is described in it — a
# description stores tracks, runs and readings — so every member the
# kernel stores, compares or evaluates is defined in the header that
# declares it, and the kernel links nothing here to do so. What the
# archive holds is the members that carry a diagnostic and the ENGINE
# behind dressed type: fx() tracks resolved and
# drawn per glyph, a run laid along a path, mark() anchors, span restyles
# folded into axis tracks, and the schedule read back — reached by the
# kernel through the TextPainter value the text verbs install on a
# description, which is why those verbs (`fx`, `onPath`, `mark`,
# `spanStyle`, `spanPaint`, `variationDrive`, `annotate`) are defined here
# though the kernel declares them. With it the kit's legibility helpers
# (kit/Legibility.h), spelled in this tier's types.
#
# Sees the kernel's source directory PRIVATELY: the engine reads the
# retained Instance and Composer::Impl the way a kernel phase does,
# because it is one. Nothing above this target inherits that path.
#
# The vocabulary's headers spell the seeded mixer an effect draws from, the
# field count a track's pin reads, the paint a pass runs, the spread a
# cascade is, and the selector, granularity and style a run is set in, so
# those libraries are PUBLIC. The engine's sources walk a geometry contour,
# stamp image assets, and shape, choreograph, decorate, lay out and query
# text through SigilWeave; none of that reaches a public header, so those
# are PRIVATE.
find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
sigil_library(SigilComposeTypography
  SOURCES
    # the vocabulary's out-of-line members
    TextEffect.cpp
    # the engine
    TextFx.cpp TextFxPainting.cpp TextAnnotations.cpp TextMarks.cpp
    PathText.cpp TextPainter.cpp AxisGate.h TextEngine.h TextPose.h
  HEADERS Annotation.h Selector.h TextEffect.h TextPath.h TextUnit.h
          Track.h Typography.h ../kit/Legibility.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilCoreComparable SigilCoreCompute SigilMaterial
         SigilMotion SigilWeave
  PRIVATE SigilCoreReconcile SigilGeometryPath SigilImage Boost::container
          Boost::unordered yoga::yogacore)

# Text data, the text pass, vertical writing, motion along paths, the
# text-fx presets, rich spans and the variation drive. The kit is here
# because a path baseline and an upright column are exercised on `shapes::`
# silhouettes. A case about an axis or a cluster shapes an instrument from
# src/test/assets/; the label is for the rest, which set unstyled type in
# whatever face the machine calls its default.
sigil_test(compose_test
  SOURCES test/ComposeTestTextData.cpp test/ComposeTestTextPass.cpp
          test/ComposeTestVertical.cpp test/ComposeTestPathMotion.cpp
          test/ComposeTestContentText.cpp test/ComposeTestParagraphs.cpp
          test/ComposeTestVariationDrive.cpp
  LIBRARIES SigilComposeTypography SigilComposeKit SigilComposeTesting
            SigilWeavePorts
  LABELS fonts)

# Dense text replayed, baked and projected; kinetic text every frame. The
# kit is here for the stock effect the kinetic arms drive.
sigil_bench(compose_bench
  SOURCES bench/ComposeTextBench.cpp
  LIBRARIES SigilComposeTypography SigilComposeKit SigilWeavePorts
  GPU)
