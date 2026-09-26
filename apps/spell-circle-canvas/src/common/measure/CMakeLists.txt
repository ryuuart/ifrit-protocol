# SigilMeasure — timing, statistics and check reporting in one library.
#
# Every other library measures itself with this one, so it has no
# domain-library dependency at all; its only outside link is
# Boost.Container, which the compact, ordered counter table is built on
# behind its implementation and which no public header includes.
#
# Every public header lives under one include root, include/sigilmeasure/:
# the tier-1 instruments under time/, stats/ and check/, gathered by
# Measure.h, and the control a caller rarely needs under advanced/.
# The reference pages: the prose a header's brief stands over, one page
# per entity, named once here and read by both the documentation site and
# the probe below — so a page that is written is a page that is checked.
set(SIGIL_MEASURE_REFERENCE
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Check.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/FrameTimer.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Histogram.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Laps.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Window.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/LineFit.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Moments.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Rescale.md)

sigil_library_root(SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox ${SIGIL_MEASURE_REFERENCE})

find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMeasure
  SOURCES check/Check.cpp stats/Counters.cpp
  HEADERS Measure.h
          check/Check.h
          stats/Histogram.h stats/Quantile.h stats/Rate.h stats/Smoothed.h
          stats/Summary.h stats/Window.h
          time/Stopwatch.h
          advanced/CheckFormat.h advanced/Counters.h advanced/FrameTimer.h
          advanced/Laps.h advanced/LineFit.h advanced/Moments.h
          advanced/Quantiles.h advanced/Rescale.h
  PRIVATE Boost::container)

sigil_test(measure_test
  SOURCES stats/test/CountersTest.cpp stats/test/FitTest.cpp
          stats/test/QuantileTest.cpp stats/test/StatsTest.cpp
          stats/test/StreamTest.cpp
          time/test/TimeTest.cpp check/test/CheckTest.cpp
  LIBRARIES SigilMeasure)

sigil_bench(measure_bench
  SOURCES stats/bench/StatsBench.cpp stats/bench/WindowBench.cpp
  LIBRARIES SigilMeasure)

# Every API name the prose spells, compiled against the headers that own
# it.
sigil_doc_probes(measure
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${SIGIL_MEASURE_REFERENCE}
  FLOORS 20,10,40,8,0
  LIBRARIES SigilMeasure)
