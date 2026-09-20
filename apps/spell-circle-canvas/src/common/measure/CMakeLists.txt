# SigilMeasure — timing, statistics and check reporting in one library.
#
# Every other library measures itself with this one, so it has no
# domain-library dependency at all; its only outside link is the
# Boost.Container header the compact, ordered counter table is built on.
#
# Every public header lives under one include root, include/sigilmeasure/.
# The reference pages: the prose a header's brief stands over, one page
# per entity, named once here and read by both the documentation site and
# the probe below — so a page that is written is a page that is checked.
set(SIGIL_MEASURE_REFERENCE
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Check.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/FrameTimer.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Histogram.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Laps.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/LineFit.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Moments.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Rescale.md)

sigil_library_root(SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox ${SIGIL_MEASURE_REFERENCE})

find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMeasure
  SOURCES check/Check.cpp
  HEADERS check/Check.h
          stats/Counters.h stats/Fit.h stats/FrameSample.h stats/Histogram.h
          stats/Moments.h stats/Rescale.h stats/Samples.h
          time/FrameTimer.h time/Laps.h time/Stopwatch.h
  PUBLIC Boost::container)

sigil_test(measure_test
  SOURCES stats/test/CountersTest.cpp stats/test/FitTest.cpp
          stats/test/SamplesTest.cpp stats/test/StatsTest.cpp
          time/test/TimeTest.cpp check/test/CheckTest.cpp
  LIBRARIES SigilMeasure)

sigil_bench(measure_bench
  SOURCES stats/bench/SamplesBench.cpp stats/bench/StatsBench.cpp
  LIBRARIES SigilMeasure)

# Every API name the prose spells, compiled against the headers that own
# it.
sigil_doc_probes(measure
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${SIGIL_MEASURE_REFERENCE}
  FLOORS 20,10,40,8,0
  LIBRARIES SigilMeasure)
