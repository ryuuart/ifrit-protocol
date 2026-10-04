# Chapters registered with both the documentation site and compile probes.
set(compose_chapters
  ${CMAKE_CURRENT_SOURCE_DIR}/TYPOGRAPHY.md
  ${CMAKE_CURRENT_SOURCE_DIR}/core/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/ELEMENTS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/VERBS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/VALUES.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/COLOURING.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/CASCADE.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/SELECTORS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/DEPTH.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/HEADERS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/CACHING.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/TRAPS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/TEXT_FX.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/TEXT_PATH.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/RICH_TEXT.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/PARAGRAPHS.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/BESIDE_TEXT.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/VERTICAL_TEXT.md)

sigil_library_root(SigilCompose
  BRIEF "Immutable value-typed descriptions of a 2D scene, laid out with flexbox and drawn to an SkCanvas"
  DOCS ${compose_chapters})

# Define dependencies before consumers, including the shared testing target.
add_subdirectory(core)
add_subdirectory(testing)
add_subdirectory(typography)
add_subdirectory(brush)
add_subdirectory(texture)
add_subdirectory(web)
add_subdirectory(draw)
add_subdirectory(kit)

# The convenience target includes the authoring tiers; hosted leaves are opt-in.
sigil_library(SigilCompose
  INTERFACE SigilComposeKit SigilComposeBrush SigilComposeTypography)

# Probe the text library's Compose-facing controls here; names owned by that
# library are excluded because its own probe compiles them.
set(weave_owned "a SigilWeave name in its catalogue's Where column, compiled by SigilWeave's own probe")
sigil_doc_probes(compose
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${compose_chapters}
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Corners.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Dimension.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Edges.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Fill.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/FontStyle.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/PaintBox.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/PathFormat.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Property.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Shadow.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/elements/box.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/elements/custom.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/elements/image.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/elements/stack.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/elements/text.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textAnnotation.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/gridArea.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/at.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/atRest.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/backdropFilter.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/background.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/cacheScale.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textThreadBalance.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/blendMode.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/paragraph.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/decorationOutline.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/gridCells.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/children.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/overflow.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/padding.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/margin.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/inset.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/borderRadius.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/cover.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/display.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/filter.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/fill.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/contentFlowAround.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/font.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/fontFamily.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/fontStyle.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/foreground.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/initialLetter.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/ink.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textWillChange.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textAttach.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textIndent.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/mask.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/opacity.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/overlay.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/paragraphStyles.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/preserve3d.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/rect.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/imageRendering.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/lighting.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/scaleX.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/shape.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/span.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/stroke.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textStroke.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/textThreadTo.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/transformOrigin.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/travel.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/variationDrive.md
          ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/PARITY.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/testing/include/sigilcompose
           ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../measure/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/kit
           ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/path
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/core
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/paint
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/filter
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/skia
           ${CMAKE_CURRENT_SOURCE_DIR}/../media/include/sigilmedia/core
           ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include/sigilskia/draw
           ${CMAKE_CURRENT_SOURCE_DIR}/../draw/include/sigildraw
           ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../test
  PRELUDES "\"support/DocsTestSupport.h\""
           "<sigilskia/draw/Direct.h>"
           "<sigilmedia/video/Video.h>"
           "<sigilmaterial/program/Shader.h>"
           "<sigilmaterial/surface/Surface.h>"
           "<sigilmaterial/texture/Image.h>"
  SKIP_HEADERS Web.h
  EXCLUDE "kit::PatternHyphenator=${weave_owned}"
          "kit::englishHyphenator=${weave_owned}"
          "kit::englishHyphenationPatterns=${weave_owned}"
          "selectors::regex=${weave_owned}"
          "selectors::line=${weave_owned}"
          "media::distanceField=${weave_owned}"
  FLOORS 280,190,150,220,5
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
            SigilComposeTypography SigilComposeDraw
            SigilComposeTesting SigilWeavePorts SigilSkiaDraw
            SigilMediaVideoDecode)

sigil_test(compose_test
  SOURCES test/ComposeTestDocs.cpp
  LIBRARIES compose_api_doc_probes SigilComposeCore SigilComposeKit
            SigilComposeBrush SigilComposeTypography
            SigilComposeDraw SigilComposeTesting SigilWeavePorts)

# Compile headers against their originating feature's own usage requirements.
add_custom_target(compose_header_self_test)
foreach(feature Core Kit Brush Typography Draw Texture Web)
  if(NOT TARGET SigilCompose${feature})
    continue()
  endif()
  string(TOLOWER ${feature} directory)
  file(GLOB_RECURSE headers CONFIGURE_DEPENDS
       ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilcompose/${directory}/*.h)
  if(feature STREQUAL Core)
    list(APPEND headers ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilcompose/Compose.h)
  elseif(feature STREQUAL Brush)
    list(APPEND headers ${CMAKE_CURRENT_SOURCE_DIR}/include/sigilcompose/advanced/PathEffect.h)
  endif()
  sigil_header_self_test(compose_${directory}_header_self_test NO_TEST
    HEADERS ${headers} LIBRARIES SigilCompose${feature})
  add_dependencies(compose_header_self_test compose_${directory}_header_self_test)
endforeach()
file(GLOB_RECURSE testing_headers CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/testing/include/sigilcompose/*.h)
sigil_header_self_test(compose_testing_header_self_test NO_TEST
  INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/testing/include
  HEADERS ${testing_headers} LIBRARIES SigilComposeTesting)
add_dependencies(compose_header_self_test compose_testing_header_self_test)
add_test(NAME compose_header_self_test
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
          --config $<CONFIG> --target compose_header_self_test)
set_tests_properties(compose_header_self_test PROPERTIES RESOURCE_LOCK sigil_build_tree)

# Device readbacks cover image operations unavailable to the raster suites.
if(APPLE)
  sigil_test(compose_test
    SOURCES test/ComposeGpuTest.mm test/ComposeGpuDestinationTest.mm
            test/ComposeGpuPromotionTest.mm
    LIBRARIES SigilComposeKit SigilComposeBrush SigilComposeTypography
              SigilWeavePorts SigilSkia
    SUITES ComposeGpu "*/DirectImageDraw" "*/ComposeGpuMaterialDestination"
      "*/ComposeGpuSpanInk"
    LABELS gpu)
endif()
