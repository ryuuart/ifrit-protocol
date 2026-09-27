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
# (the `textFx::` presets over the typography tier's effect seam in
# kit/Kinetic.h, with the marquee beside it in kit/Marquee.h), and HOW a
# pool is filled (the placers). The
# silhouette generators are the geometry library's, because a
# `path(SkSize)` value needs nothing of a component tree.
#
# Three kit headers live with other tiers instead, because their values are
# spelled in those tiers' types: `kit/Strokes.h` (the stroke grammar) and
# `kit/Plate.h` (the bordered feed plate) with Brush, and `kit/Legibility.h`
# with Typography. Include them directly.
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
  SOURCES Bevels.cpp Board.cpp Connect.cpp Document.cpp Frame.cpp
          Instruments.cpp Layouts.cpp Outline.cpp Pin.cpp PixelType.cpp Routers.cpp Rows.cpp Specimen.cpp
          Sprites.cpp
          Stamp.cpp
  HEADERS Board.h Chrome.h Connect.h Document.h Feed.h Frame.h
          Annotations.h Outline.h Pin.h Stamp.h
          Typeset.h Instruments.h Kinetic.h Kit.h Layouts.h Marquee.h
          PixelType.h Placers.h Routers.h Rows.h Specimen.h Sprites.h
  PUBLIC SigilComposeCore SigilComposeTypography SigilComposeBrush
         SigilGeometryKit SigilCoreCompute SigilMaterial SigilWeave SigilWeavePorts)

# The kit's cases, declared here beside the code they cover and carried
# by the library's one test binary. SigilMeasure is here for the checks a
# feed plate reports, which is the one target the kit's own link line
# does not carry.
#
# Every case sets the instrument face from src/test/assets/ except the one
# that asks the machine's italic families whether their ink overhangs, so
# that case alone carries the label.
sigil_test(compose_test
  SOURCES test/ComposeTestBoard.cpp test/ComposeTestChrome.cpp
          test/ComposeTestColumns.cpp test/ComposeTestDocument.cpp
          test/ComposeTestRules.cpp test/ComposeTestFeed.cpp
          test/ComposeTestFeedPlate.cpp
          test/ComposeTestFlowAround.cpp test/ComposeTestFrame.cpp
          test/ComposeTestLayouts.cpp
          test/ComposeTestConnect.cpp test/ComposeTestAdders.cpp
          test/ComposeTestLegibility.cpp test/ComposeTestOrnament.cpp
          test/ComposeTestPixelType.cpp test/ComposeTestPlacers.cpp
          test/ComposeTestQueries.cpp test/ComposeTestRouters.cpp
          test/ComposeTestRows.cpp test/ComposeTestShapeValues.cpp
          test/ComposeTestSpecimen.cpp test/ComposeTestSprites.cpp
          test/ComposeTestStudio.cpp test/ComposeTestTiling.cpp
          test/ComposeTestTravel.cpp test/ComposeTestFactsAcrossImages.cpp
  LIBRARIES SigilComposeKit SigilComposeBrush SigilMeasure
            SigilComposeTesting SigilWeavePorts
  LABELS fonts
  SUITES KitPixelType.InkReallyDoesOverhangTheAdvanceSoThePadIsLoadBearing)

# A GUEST IMAGE that states a fact of a library type, compiled hidden as the
# live host compiles a sketch, so its identity for the type is its own, and
# resolving the library out of the test binary as a sketch resolves it out
# of Sketchbook.
if(TARGET compose_test)
  add_library(compose_test_pin_guest MODULE test/guest/PinStatingGuest.cpp)
  target_link_libraries(compose_test_pin_guest PRIVATE
    $<COMPILE_ONLY:SigilComposeKit>)
  set_target_properties(compose_test_pin_guest PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  if(APPLE)
    target_link_options(compose_test_pin_guest PRIVATE
      -undefined dynamic_lookup)
  endif()
  add_dependencies(compose_test compose_test_pin_guest)
  # The binary is created by the library's first call, in another
  # directory, and a source's properties are read in the target's.
  set_source_files_properties(test/ComposeTestFactsAcrossImages.cpp
    TARGET_DIRECTORY compose_test PROPERTIES
    COMPILE_DEFINITIONS
      "SIGIL_COMPOSE_PIN_GUEST=\"$<TARGET_FILE:compose_test_pin_guest>\"")
endif()

# Comparable silhouettes, hit testing and blended shaped leaves.
sigil_bench(compose_bench
  SOURCES bench/ComposeChromeBench.cpp bench/ComposeShapeBench.cpp
          bench/ComposeSpriteBench.cpp
  LIBRARIES SigilComposeKit SigilWeavePorts)
