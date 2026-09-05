# SigilComposeKit — the KIT tier as its own library.
#
# The kit plugs public seams and changes no kernel semantics. This target
# makes that STRUCTURAL rather than a convention: it links the kernel and
# the Typography and Brush tiers, so it sees their PUBLIC headers, and nothing adds
# the compose source root to its include path — so ComposeInternal.h and
# ComposeRuntime.h are unreachable from kit code.
#
# The shelves of stock values are here: WHERE a child lands (`layouts::`),
# WHAT JOINS two of them (`routers::`), HOW a glyph deviates from rest
# (the `fx::` presets over the typography tier's effect seam, with the
# marquee, in kit/Kinetic.h), and HOW a pool is filled (the placers). The
# silhouette generators are the geometry library's, because a
# `path(SkSize)` value needs nothing of a component tree.
#
# Five kit headers live with other tiers instead, because their values are
# spelled in those tiers' types: `kit/Strokes.h` (the stroke grammar),
# `kit/Plate.h` (the bordered feed plate), `kit/Ornament.h` and
# `kit/Flourish.h` (the pieces a manuscript border is made of) with Brush,
# and `kit/Legibility.h` with Typography. Include them directly.
#
# PUBLIC (not PRIVATE): anything that links the kit gets the tiers its values
# are expressed in — the kernel, type styles, the brush tier whose
# decorations the era looks are built from, and the geometry library's
# silhouette shelf — with it, and the libraries the kit headers include
# themselves: the material tables and paints the chrome, gel and gloss
# looks are drawn with, the type style a typeset takes, and the seeded
# mixer a layout scatters with. The arrow points one way: this tier links
# Brush, and Brush links nothing here.
sigil_library(SigilComposeKit
  SOURCES Eras.cpp Kit.cpp Routers.cpp
  HEADERS Chrome.h Frame.h Gel.h Gloss.h Annotations.h Typeset.h
          Instruments.h Kinetic.h Kit.h Layouts.h PixelType.h Placers.h
          Routers.h Specimen.h Sprites.h
  PUBLIC SigilComposeCore SigilComposeTypography SigilComposeBrush
         SigilGeometryKit SigilCoreCompute SigilMaterial SigilWeave)

# The kit tier's tests. Separate from the kernel binaries because the kit
# changes nothing in the kernel, so a kit failure must never look like a
# kernel failure on the dashboard. One binary for the tier rather than
# three: the kit PUBLIC-links Brush, so a binary over Kit and a binary
# over Kit+Brush have the same closure and the split states no boundary a
# consumer could read. SigilMeasure is here for the checks a feed plate
# reports, which is the one target the kit's own link line does not carry.
#
# Shapes text through the machine's own faces, so the label says so.
sigil_test(compose_kit_test
  SOURCES test/ComposeTestKitValues.cpp test/ComposeTestLayoutsAndShapes.cpp
          test/ComposeTestTravel.cpp test/ComposeTestRouters.cpp
          test/ComposeTestPlacers.cpp test/ComposeTestQueries.cpp
          test/ComposeTestStudio.cpp
  LIBRARIES SigilComposeKit SigilComposeBrush SigilMeasure
            SigilComposeTesting SigilWeavePorts
  LABELS fonts)

# Comparable silhouettes, hit testing and blended shaped leaves.
sigil_bench(compose_shape_bench
  SOURCES bench/ComposeShapeBench.cpp
  LIBRARIES SigilComposeKit SigilWeavePorts)
