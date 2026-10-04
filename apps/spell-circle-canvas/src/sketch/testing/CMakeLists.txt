# The testing tier: a sketch host in the test's own process, spoken to
# through the protocol exactly as a script speaks to one over a socket,
# and the harness a GoogleTest case drives it through.
#
# SigilSketchTesting is the host and the comparison, with no test
# framework in it, so Python's in-process route binds it as it stands;
# SigilSketchTestingHarness is the fixture, which is GoogleTest's.
find_package(GTest CONFIG REQUIRED)
sigil_library(SigilSketchTesting
  SOURCES InProcessHost.cpp Comparison.cpp
  HEADERS InProcessHost.h Comparison.h
  PUBLIC SigilSketchSessionAgent SigilMediaDifference
  PRIVATE SigilMediaImageDecode SigilIOSource SigilWeavePorts)

sigil_library(SigilSketchTestingHarness
  SOURCES Harness.cpp
  HEADERS Harness.h
  PUBLIC SigilSketchTesting GTest::gtest)

# Fixture contracts use protocol clients; scene cases exercise the complete
# registry, session, clock and still-capture path.
sigil_test(sketch_test
  SOURCES test/HarnessTest.cpp
  LIBRARIES SigilSketchTestingHarness
  SUITES HarnessHooks HarnessFailure
  LABELS protocol)
sigil_test(sketch_test SUITES Harness LABELS protocol e2e)
