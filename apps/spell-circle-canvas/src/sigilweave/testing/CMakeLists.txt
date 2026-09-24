# The library's own harness: a passage laid under a stated font context,
# read back as values — lines with their boxes, fit and badness, runs,
# glyph placements, hyphenation points — rendered to a raster plate on the
# CPU, and held against a committed baseline. Its own target, so a test
# binary links it and nothing that ships does. The walk over placed glyphs
# is the choreograph feature's; decoding and encoding a baseline are
# SigilImage's and reading and writing its file SigilIO's, all private.
sigil_library(SigilWeaveTesting
  SOURCES Passage.cpp Reading.cpp Plate.cpp Difference.cpp Baseline.cpp
  HEADERS Passage.h Reading.h Plate.h Difference.h Baseline.h
  PUBLIC SigilWeaveLayout
  PRIVATE SigilWeavePaint SigilWeaveChoreograph SigilImageAsset
          SigilImageEncode SigilIOSource)

# The harness's own readings, over the instrument faces, and the four
# things a baseline comparison can find, over a scratch directory.
sigil_test(weave_test
  SOURCES test/ReadingTest.cpp test/BaselineTest.cpp
  LIBRARIES SigilWeaveTesting SigilWeavePorts)

# THE PLATES: eight panels of what only this library draws, each a case
# that renders through the harness and holds the picture against the PNG
# committed under test/plates/. They set the machine's own families —
# CJK, Hangul, the fallback chain, named faces — so they carry `fonts`
# beside `plates`, and a machine whose font set differs from the one the
# baselines were adopted on reads them as moved. SIGIL_PLATES_REBASE=1 in
# the environment adopts every render as its baseline instead of judging
# it; a render that does not match is written under the build tree, and
# the failure line names both files.
sigil_test(weave_test
  SOURCES test/ExtremeGeometriesPlateTest.cpp test/TypographyPlateTest.cpp
          test/ScriptConfettiPlateTest.cpp test/OpenTypeFeaturesPlateTest.cpp
          test/CjkColumnsPlateTest.cpp test/PathExclusionsPlateTest.cpp
          test/CjkFallbackPlateTest.cpp test/RunDressingPlateTest.cpp
  LIBRARIES SigilWeaveTesting SigilWeavePorts SigilWeaveKit SigilWeaveQuery
            SigilMaterialKit SigilMaterialSkia
  SUPPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../examples/shared
  DEFINITIONS
    SIGIL_WEAVE_PLATE_BASELINES="${CMAKE_CURRENT_SOURCE_DIR}/test/plates"
    SIGIL_WEAVE_PLATE_RENDERS="${CMAKE_CURRENT_BINARY_DIR}/plates"
  LABELS plates fonts
  SUITES WeavePlates)
