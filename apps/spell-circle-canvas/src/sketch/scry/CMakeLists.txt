# The shared web engine, settling a page on the engine's own events, and
# the sequence a page is put through either way — what a HOST taking a
# deterministic still of a view needs, and what one that must keep
# drawing while the page arrives needs, saying nothing about what any
# page looks like.
#
# One engine at a time stands over the process's renderer, so each suite
# here holds one engine for its cases and lets it go when it ends, and a
# later suite in the same process boots its own.
sigil_test(sketch_test
  SOURCES test/SharedEngineTest.cpp test/SettledPageTest.cpp
          test/SettlingTest.cpp
  LIBRARIES SigilSketch
  SUITES SketchSharedWebEngine SketchSettledPage SketchSettling
  LABELS ultralight)
ultralight_copy_resources(sketch_test)

# A page under the loading clock, over the session and clock agents: it
# is there before the open is answered, and its still is the sweep's
# plate. Under the protocol's label beside the engine's.
sigil_test(sketch_test
  SOURCES test/LoadingClockTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting SigilComposeWeb
  SUITES SketchLoadingClock
  LABELS ultralight protocol)

# THE SUITES TOGETHER, in one process and in the order the binary runs
# them. ctest runs every discovered case in a process of its own, which
# would hide an engine one suite kept from the next; here the page under
# the loading clock comes last and must still arrive before its setup
# returns.
add_test(NAME sketch_web_suites_in_one_process
  COMMAND sketch_test
    "--gtest_filter=SketchSharedWebEngine.*:SketchSettledPage.*:SketchSettling.*:SketchLoadingClock.*")
set_tests_properties(sketch_web_suites_in_one_process PROPERTIES
  LABELS "ultralight;protocol")
