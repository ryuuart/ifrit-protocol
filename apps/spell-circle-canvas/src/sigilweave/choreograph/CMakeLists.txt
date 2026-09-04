# Per-glyph choreography over a finished layout: the walk that hands every
# placed glyph to an effect with its rest pose, paint and identity, the
# dressing a glyph draws with, and the batches that collapse thousands of
# animated letters into a few drawGlyphsRSXform calls.
add_library(SigilWeaveChoreograph STATIC
  Choreograph.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/choreograph/PlacedGlyph.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/choreograph/GlyphDress.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/choreograph/GlyphBatches.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/choreograph/Choreograph.h
)
target_compile_features(SigilWeaveChoreograph PUBLIC cxx_std_20)
target_include_directories(SigilWeaveChoreograph PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveChoreograph
  PUBLIC
    SigilWeaveLayout
  PRIVATE
    Boost::container
)

# The walk, the batches, and a glyph on a contour re-placed from its pen.
add_executable(weave_choreograph_test
  test/ChoreographTest.cpp
)
target_include_directories(weave_choreograph_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_choreograph_test PRIVATE
  SigilWeaveChoreograph
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_choreograph_test COMMAND weave_choreograph_test)
