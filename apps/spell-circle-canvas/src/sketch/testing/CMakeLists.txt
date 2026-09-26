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

# The harness's own seams, under the protocol's one label: the hooks in
# their order, and the host's account printed ahead of a case's first
# failure. Beside them, end to end, a still the harness takes is the
# plate the sweep writes of the same scene at the same moment.
sigil_test(sketch_test
  SOURCES test/HarnessTest.cpp
  LIBRARIES SigilSketchTestingHarness
  SUITES HarnessHooks HarnessFailure Harness.TheReadoutIsDescribeTheClockAndTheLastStill
  LABELS protocol)
