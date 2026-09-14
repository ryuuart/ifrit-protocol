# The shared web engine, settling a page on the engine's own events, and
# the sequence a page is put through either way — what a HOST taking a
# deterministic still of a view needs, and what one that must keep
# drawing while the page arrives needs, saying nothing about what any
# page looks like.
#
# One engine at a time stands over the process's renderer, and each case
# here boots its own, so they must not meet in one process. ctest runs
# every discovered case in a process of its own, which is what keeps them
# apart, since the library has one binary.
sigil_test(sketch_test
  SOURCES test/SharedEngineTest.cpp test/SettledPageTest.cpp
          test/SettlingTest.cpp
  LIBRARIES SigilSketch
  SUITES SketchSharedWebEngine SketchSettledPage SketchSettling
  LABELS ultralight)
ultralight_copy_resources(sketch_test)
