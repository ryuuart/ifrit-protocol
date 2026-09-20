# SigilMeasure — timing, statistics and check reporting in one library.
#
# Every other library measures itself with this one, so it has no
# domain-library dependency at all; its only outside link is the
# Boost.Container header the compact, ordered counter table is built on.
#
# Every public header lives under one include root, include/sigilmeasure/.
sigil_library_root(SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox)

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
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  FLOORS 2,0,0,0,0
  LIBRARIES SigilMeasure)
