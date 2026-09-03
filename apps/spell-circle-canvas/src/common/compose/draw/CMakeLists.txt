# The draw feature: the door between the declarative tree and the
# imperative pen, both ways — `draw(program)`, a node whose content is a
# pen program, and `paintRetained`, which is where `pen.element(...)`
# lands: a composer kept in the pen for the call site, reconciled every
# frame. Links the kernel it makes nodes with and SigilDraw, the pen; the
# clock and ticker a guest composer runs on are read in its source alone.
add_library(SigilComposeDraw STATIC
  Draw.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/draw/Draw.h
)
target_compile_features(SigilComposeDraw PUBLIC cxx_std_20)
target_link_libraries(SigilComposeDraw PUBLIC SigilComposeCore SigilDraw)
target_link_libraries(SigilComposeDraw PRIVATE SigilMotionClock)
set_target_properties(SigilComposeDraw PROPERTIES POSITION_INDEPENDENT_CODE ON)

_compose_test_binary(compose_draw_test
  LIBRARIES SigilComposeDraw
  SOURCES test/ComposeTestDraw.cpp
)

_compose_bench(compose_draw_bench
  LIBRARIES SigilComposeDraw
  SOURCES bench/DrawBench.cpp
)
