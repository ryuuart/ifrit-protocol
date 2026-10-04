sigil_library(SigilComposeKit
  SOURCES Bevels.cpp Board.cpp Connect.cpp Document.cpp Frame.cpp
          Instruments.cpp Layouts.cpp Outline.cpp Pin.cpp PixelType.cpp Routers.cpp Rows.cpp Specimen.cpp
          Sprites.cpp
          Stamp.cpp
  HEADERS Board.h Chrome.h Connect.h Document.h Feed.h Frame.h
          Annotations.h Outline.h Pin.h Stamp.h
          Typeset.h Instruments.h Kit.h Layouts.h Marquee.h
          PixelType.h Placers.h Routers.h Rows.h Specimen.h Sprites.h
  # Bevels reuse private kernel material lowering and coverage painting.
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilComposeTypography SigilComposeBrush
         SigilGeometryKit SigilCoreCompute SigilMaterial SigilWeave SigilWeavePorts)

# The italic-overhang case requires system fonts; other cases use instruments.
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

# The hidden guest resolves host symbols to exercise facts across loaded images.
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
  # Source properties must be set in the directory that created the test target.
  set_source_files_properties(test/ComposeTestFactsAcrossImages.cpp
    TARGET_DIRECTORY compose_test PROPERTIES
    COMPILE_DEFINITIONS
      "SIGIL_COMPOSE_PIN_GUEST=\"$<TARGET_FILE:compose_test_pin_guest>\"")
endif()

sigil_bench(compose_bench
  SOURCES bench/ComposeChromeBench.cpp bench/ComposeShapeBench.cpp
          bench/ComposeSpriteBench.cpp
  LIBRARIES SigilComposeKit SigilWeavePorts)
