sigil_library(SigilComposeDraw
  SOURCES Draw.cpp
  HEADERS Draw.h
  PUBLIC SigilComposeCore SigilDraw
  PRIVATE SigilMotionClock)

sigil_test(compose_test
  SOURCES test/ComposeTestDraw.cpp test/ComposeTestGraphics.cpp
          test/ComposeTestMaterialInput.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../draw/test
  LIBRARIES SigilComposeDraw SigilComposeTesting SigilDrawBrush
            SigilMaterialProgram SigilMaterialSkia SigilWeavePorts
  SUITES DrawNode GraphicsNode MaterialInput RetainedElement
         PenAndGraphics/DrawProgramRecovery
  LABELS integration)

sigil_bench(compose_bench
  SOURCES bench/DrawBench.cpp
  LIBRARIES SigilComposeDraw SigilWeavePorts)
