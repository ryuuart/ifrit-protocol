# SigilMeasure — timing, statistics and check reporting. Every other library
# measures itself with this one, so its sole dependency is the Boost.Container
# header used by the compact, ordered counter table.
#
# Every public header lives under one include root, include/sigilmeasure/
# <subject>/, and <sigilmeasure/Measure.h> is the umbrella over all of
# them. One static target: the headers are inline, and the one source
# file holds the table formatting.
sigil_library_root(SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage")

find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMeasure
  SOURCES check/Check.cpp
  HEADERS Measure.h check/Check.h stats/Counters.h stats/FrameSample.h
          stats/Samples.h time/FrameTimer.h time/Laps.h time/Stopwatch.h
  PUBLIC Boost::container)

# Links only the library it exercises, so a link edge that pulled anything
# else in would show up as a build failure here.
sigil_test(measure_test
  SOURCES test/MeasureTest.cpp
  LIBRARIES SigilMeasure)

sigil_bench(measure_bench
  SOURCES bench/MeasureBench.cpp
  LIBRARIES SigilMeasure)
