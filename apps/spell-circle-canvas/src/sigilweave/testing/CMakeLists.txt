# The library's own harness: a passage laid under a stated font context,
# read back as values — lines with their boxes, fit and badness, runs,
# glyph placements, hyphenation points — rendered to a raster plate on the
# CPU, and held against a committed baseline. Its own target, so a test
# binary links it and nothing that ships does. The pixel difference a
# comparison answers is SigilImage's, public because the comparison names
# it; the walk over placed glyphs is the choreograph feature's, decoding
# and encoding a baseline SigilImage's and reading and writing its file
# SigilIO's, all private.
sigil_library(SigilWeaveTesting
  SOURCES Passage.cpp Reading.cpp Plate.cpp Baseline.cpp
  HEADERS Passage.h Reading.h Plate.h Baseline.h
  PUBLIC SigilWeaveLayout SigilImageDifference
  PRIVATE SigilWeavePaint SigilWeaveChoreograph SigilImageAsset
          SigilImageEncode SigilIOSource)

# The harness's own readings, over the instrument faces, and the four
# things a baseline comparison can find, over a scratch directory.
sigil_test(weave_test
  SOURCES test/ReadingTest.cpp test/BaselineTest.cpp
  LIBRARIES SigilWeaveTesting SigilWeavePorts)

# THE PLATES: eight panels of what only this library draws, each a case
# that renders through the harness and holds the picture against the PNG
# committed under test/plates/, with the faces it was drawn in listed
# beside it. They set the machine's own families — CJK, Hangul, the
# fallback chain, named faces — so they carry `fonts` beside `plates`, and
# a machine whose font set differs from the one the baselines were adopted
# on reads them as a changed font set rather than a moved layout.
# SIGIL_PLATES_REBASE=1 in the environment, which `sigil.py plates
# --rebase` sets, adopts every render as its baseline instead of judging
# it; a render that does not match is written under the build tree, and
# the failure line names both files.
sigil_test(weave_test
  SOURCES test/ExtremeGeometriesPlateTest.cpp test/TypographyPlateTest.cpp
          test/ScriptConfettiPlateTest.cpp test/OpenTypeFeaturesPlateTest.cpp
          test/CjkColumnsPlateTest.cpp test/PathExclusionsPlateTest.cpp
          test/CjkFallbackPlateTest.cpp test/RunDressingPlateTest.cpp
  LIBRARIES SigilWeaveTesting SigilWeavePorts SigilWeaveKit SigilWeaveQuery
            SigilMaterialKit SigilMaterialSkia
  DEFINITIONS
    SIGIL_WEAVE_PLATE_BASELINES="${CMAKE_CURRENT_SOURCE_DIR}/test/plates"
    SIGIL_WEAVE_PLATE_RENDERS="${CMAKE_CURRENT_BINARY_DIR}/plates"
  LABELS plates fonts
  SUITES WeavePlates)
