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
# CanvasSpec, and links no device, no runtime and no sketch. PIC, because
# a hot-reloaded sketch's dylib force-loads it out of the host.
sigil_library(SigilSketchKit PIC
  SOURCES Cells.cpp Page.cpp Passage.cpp Theme.cpp
  HEADERS Cells.h Kit.h Page.h Passage.h Theme.h
  PUBLIC SigilSketchCanvas SigilComposeCore SigilComposeKit SigilCoreReconcile
         SigilWeave
  PRIVATE SigilWeavePorts SigilIO)

# The kit's own claim: the theme is a comparable value a scope binds, and
# every component draws exactly what the compose kit spelled by hand with
# the same values draws — asserted in pixels, because that is the claim a
# migrated sketch's plate rests on.
sigil_test(sketch_kit_test
  SOURCES test/SketchKitTest.cpp
  LIBRARIES SigilSketchKit SigilMotionClock)
