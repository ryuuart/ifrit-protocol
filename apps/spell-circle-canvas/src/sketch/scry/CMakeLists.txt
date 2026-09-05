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

# The engine allows one renderer per process, and the shared-engine case
# ends by shutting its one down for good, so these two cases must not meet
# in one process. ctest runs every discovered case in a process of its own,
# which is what keeps them apart now that the library has one binary.
sigil_test(sketch_test
  SOURCES test/SharedEngineTest.cpp test/SettledPageTest.cpp
  LIBRARIES SigilSketchScry
  SUITES SketchSharedWebEngine SketchSettledPage LABELS ultralight)
ultralight_copy_resources(sketch_test)
