# Decoration bands resolved against placed runs: thickness, position and
# color filled in from the font's metrics, the paint a band draws with,
# skip-ink cut around glyph intercepts, and the walk that turns a layout's
# decorations into rectangles the paint feature puts on the canvas.
add_library(SigilWeaveDecoration STATIC
  Decoration.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/decoration/Decoration.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/decoration/DecorationRects.h
)
target_compile_features(SigilWeaveDecoration PUBLIC cxx_std_20)
target_include_directories(SigilWeaveDecoration PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveDecoration
  PUBLIC
    SigilWeaveLayout
)

# Bands checked as geometry, without drawing them.
add_executable(weave_decoration_test
  test/DecorationTest.cpp
)
target_include_directories(weave_decoration_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_decoration_test PRIVATE
  SigilWeaveDecoration
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_decoration_test COMMAND weave_decoration_test)
