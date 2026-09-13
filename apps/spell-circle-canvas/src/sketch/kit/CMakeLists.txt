# SigilSketchKit — the sheet a sketch stands on: the furniture every
# specimen sheet would otherwise retype, and one theme value that carries
# the look down the tree.
#
# IT COMPOSES AND DECIDES NOTHING. Every component is plain composition
# over the compose kit's arrangement with the theme's values put in, and
# the theme is a SEAM — a comparable struct a scope binds — over which
# `houseTheme()` is one stock value among peers. A sketch's own theme is
# indistinguishable from it at every call site, and a component with no
# theme bound behaves correctly.
#
# It stands above the canvas runtime because `stage()` writes a
# CanvasSpecification through a SketchContext, which is why SigilSketch is PUBLIC
# here; it links no device backend, no window and no sketch.
sigil_library(SigilSketchKit
  SOURCES Cells.cpp Chart.cpp ChartMarks.cpp Console.cpp Document.cpp
          Heading.cpp Legend.cpp Meter.cpp Page.cpp Panel.cpp Passage.cpp
          Rows.cpp Scrollbar.cpp Theme.cpp Ticker.cpp
  HEADERS Cells.h Chart.h Console.h Document.h Heading.h Kit.h Legend.h
          Meter.h Page.h Panel.h Passage.h Rows.h Scrollbar.h Theme.h Ticker.h
  PUBLIC SigilSketch SigilComposeCore SigilComposeKit SigilCoreReconcile
         SigilDataDecode SigilDataScale SigilGeometryKit SigilMaterial
         SigilWeave
  PRIVATE SigilWeavePorts SigilIO SigilDataTable)

# The kit's own claim: the theme is a comparable value a scope binds, and
# every component draws exactly what the compose kit spelled by hand with
# the same values draws — asserted in pixels, because that is the claim a
# migrated sketch's plate rests on.
sigil_test(sketch_test
  SOURCES test/CellsTest.cpp test/ChartTest.cpp test/ConsoleTest.cpp
          test/DocumentTest.cpp
          test/HeadingTest.cpp test/LegendTest.cpp test/MeterTest.cpp
          test/PageTest.cpp test/PanelTest.cpp test/PassageTest.cpp
          test/RowsTest.cpp test/ScrollbarTest.cpp test/ThemeTest.cpp
          test/TickerTest.cpp
  LIBRARIES SigilSketchKit SigilMaterialKit SigilMotionClock SigilDataTable
  # The chart's class table is documented in the README beside this file,
  # and the README is the canon, so the case that checks the table against
  # `Theme::styleSheet()` reads the document itself.
  DEFINITIONS SIGIL_SKETCH_KIT_README="${CMAKE_CURRENT_SOURCE_DIR}/README.md")
