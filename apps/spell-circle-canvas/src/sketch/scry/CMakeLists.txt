# The optional SigilSketch × SigilScry bridge. A host configures one lazy
# process-owned engine and sketches borrow it; ordinary SigilScry consumers
# continue to construct and own WebEngine directly.
#
# Settling a page on the engine's own events belongs here for the same
# reason: it is what a HOST taking a deterministic still of a view needs,
# and it says nothing about what any page looks like.
sigil_library(SigilSketchScry PIC
  SOURCES SharedEngine.cpp
  HEADERS SettledPage.h SharedEngine.h
  PUBLIC SigilScry)

sigil_test(sketch_scry_test
  SOURCES test/SharedEngineTest.cpp
  LIBRARIES SigilSketchScry
  LABELS ultralight)
ultralight_copy_resources(sketch_scry_test)

# A binary of its own because the engine allows one renderer per process
# and the shared-engine case above ends by shutting its one down for good.
sigil_test(sketch_settled_test
  SOURCES test/SettledPageTest.cpp
  LIBRARIES SigilSketchScry
  LABELS ultralight)
ultralight_copy_resources(sketch_settled_test)
