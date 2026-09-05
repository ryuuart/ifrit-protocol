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

# The kit tier's tests. Separate binary rather than more cases in the
# kernel binaries: the kit changes nothing in the kernel, so a kit failure
# must never look like a kernel failure on the dashboard. Links the kit
# ALONE — the stroke-grammar cases, which need the Brush tier, are the
# brush binary's.
sigil_test(compose_kit_test
  SOURCES test/ComposeKitTest.cpp
  LIBRARIES SigilComposeKit SigilComposeTesting SigilWeavePorts)

# Composer queries, the studio, the feed plate and the instruments that
# read a cascade back — the kit's instruments over the kernel's queries,
# with Brush for the plate and SigilMeasure for the checks a plate reports.
sigil_test(compose_studio_test
  SOURCES test/ComposeTestQueriesStudio.cpp test/ComposeTestContentStudio.cpp
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush SigilMeasure
            SigilComposeTesting SigilWeavePorts)

# Silhouettes over the kernel's shape seam, the layout schemes, the
# routers, the placers that fill an instanced leaf's pool, rails and
# travel along a path — the shelves this tier adds.
sigil_test(compose_shape_test
  SOURCES test/ComposeTestContentShape.cpp test/ComposeTestKernelShape.cpp
          test/ComposeTestLinesShape.cpp test/ComposeTestPlacers.cpp
  LIBRARIES SigilComposeKit SigilComposeBrush SigilComposeTesting
            SigilWeavePorts)

# Comparable silhouettes, hit testing and blended shaped leaves.
sigil_bench(compose_shape_bench
  SOURCES bench/ComposeShapeBench.cpp
  LIBRARIES SigilComposeKit SigilWeavePorts)
