# The shared web engine, and settling a page on the engine's own events —
# what a HOST taking a deterministic still of a view needs, saying
# nothing about what any page looks like.
#
# The engine allows one renderer per process, and the shared-engine case
# ends by shutting its one down for good, so these two cases must not meet
# in one process. ctest runs every discovered case in a process of its own,
# which is what keeps them apart now that the library has one binary.
sigil_test(sketch_test
  SOURCES test/SharedEngineTest.cpp test/SettledPageTest.cpp
  LIBRARIES SigilSketch
  SUITES SketchSharedWebEngine SketchSettledPage LABELS ultralight)
ultralight_copy_resources(sketch_test)
