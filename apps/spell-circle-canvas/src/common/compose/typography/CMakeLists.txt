find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
sigil_library(SigilComposeTypography
  SOURCES
    # Out-of-line vocabulary members.
    TextEffect.cpp
    # Text execution.
    TextStructure.cpp TextSelection.cpp TextSchedule.cpp TextFx.cpp
    TextSubstitution.cpp TextUnitBox.cpp TextFxPainting.cpp TextInkUnits.cpp
    TextAnnotations.cpp TextMarks.cpp
    # Stock legibility underlays.
    Legibility.cpp
    PathText.cpp TextPainter.cpp AxisGate.h TextEngine.h TextPose.h
    TextSubstitution.h
  HEADERS Annotation.h Entrance.h Presets.h Selector.h TextEffect.h TextFx.h
          TextPath.h TextUnit.h Track.h Typography.h ../kit/Legibility.h
  # The text executor reads retained kernel state through private seams.
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilCoreComparable SigilCoreCompute SigilMaterial
         SigilMotion SigilWeave
  PRIVATE SigilCoreReconcile SigilGeometryPath SigilMediaCore Boost::container
          Boost::unordered yoga::yogacore)

# Vertical prose requires a system CJK face; other cases use instrument fonts.
sigil_test(compose_test
  SOURCES test/ComposeTestTextData.cpp test/ComposeTestTextPass.cpp
          test/ComposeTestVertical.cpp test/ComposeTestPathMotion.cpp
          test/ComposeTestPathEcho.cpp
          test/ComposeTestParagraphs.cpp test/ComposeTestUnits.cpp
          test/ComposeTestTypeset.cpp test/ComposeTestAnnotations.cpp
          test/ComposeTestStory.cpp test/ComposeTestLiveText.cpp
          test/ComposeTestGlyphBoundary.cpp
          test/ComposeTestJustification.cpp test/ComposeTestLineSetting.cpp
          test/ComposeTestVariationDrive.cpp
          test/ComposeTestTextFeed.cpp test/ComposeTestTextLeaf.cpp
          test/ComposeTestKinetic.cpp test/ComposeTestTextFx.cpp
          test/ComposeTestTextFxCombinators.cpp
          test/ComposeTestTextFxColor.cpp
          test/ComposeTestTextFxTransforms.cpp
          test/ComposeTestTextFxSchedule.cpp test/ComposeTestTextSpans.cpp
          test/ComposeTestNamedStyles.cpp test/ComposeTestInkUnits.cpp
          test/ComposeTestSpanLighting.cpp
          test/ComposeTestTextSpanAxis.cpp test/ComposeTestTextSlot.cpp
          test/ComposeTestTextMarks.cpp
  LIBRARIES SigilComposeTypography SigilComposeKit SigilComposeTesting
            SigilWeavePorts
  LABELS fonts
  SUITES TextVertical)

sigil_bench(compose_bench
  SOURCES bench/ComposeTextBench.cpp
  LIBRARIES SigilComposeTypography SigilComposeKit SigilWeavePorts
  GPU_LIBRARIES SigilSkia)
