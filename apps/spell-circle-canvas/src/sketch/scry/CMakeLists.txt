# The optional SigilSketch × SigilScry bridge. A host configures one lazy
# process-owned engine and sketches borrow it; ordinary SigilScry consumers
# continue to construct and own WebEngine directly.
sigil_library(SigilSketchScry PIC
  SOURCES SharedEngine.cpp
  HEADERS SharedEngine.h
  PUBLIC SigilScry)

sigil_test(sketch_scry_test
  SOURCES test/SharedEngineTest.cpp
  LIBRARIES SigilSketchScry)
ultralight_copy_resources(sketch_scry_test)
