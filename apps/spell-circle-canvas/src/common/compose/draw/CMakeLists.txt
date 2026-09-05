# The draw feature: the door between the declarative tree and the
# imperative pen, both ways — `draw(program)`, a node whose content is a
# pen program, and `paintRetained`, which is where `pen.element(...)`
# lands: a composer kept in the pen for the call site, reconciled every
# frame. Links the kernel it makes nodes with and SigilDraw, the pen; the
# clock and ticker a guest composer runs on are read in its source alone.
sigil_library(SigilComposeDraw PIC
  SOURCES Draw.cpp
  HEADERS Draw.h
  PUBLIC SigilComposeCore SigilDraw
  PRIVATE SigilMotionClock)

sigil_test(compose_draw_test
  SOURCES test/ComposeTestDraw.cpp
  LIBRARIES SigilComposeDraw SigilComposeTesting SigilWeavePorts)

sigil_bench(compose_draw_bench
  SOURCES bench/DrawBench.cpp
  LIBRARIES SigilComposeDraw SigilWeavePorts)
