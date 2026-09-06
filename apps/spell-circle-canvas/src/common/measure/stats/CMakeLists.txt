# The statistics: the rolling ring and the one quantile everything shares,
# the running moments a run of any length is summarised by, the histogram
# that says what shape its spread has, the straight-line map that puts two
# runs of different units on a common footing, the least-squares line, the
# named counters, and the plain numbers a frame-budget gate judges by.
#
# Header-only, so the target is an interface: the whole of it is inline
# arithmetic over the caller's own numbers. Boost.Container carries the
# flat, ordered map the counter table is, which is the one outside name
# reaching a public header here.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilMeasureStats
  HEADERS Counters.h Fit.h FrameSample.h Histogram.h Moments.h Rescale.h
          Samples.h
  INTERFACE Boost::container)

sigil_test(measure_test
  SOURCES test/CountersTest.cpp test/FitTest.cpp test/SamplesTest.cpp
          test/StatsTest.cpp
  LIBRARIES SigilMeasureStats)

sigil_bench(measure_bench
  SOURCES bench/SamplesBench.cpp bench/StatsBench.cpp
  LIBRARIES SigilMeasureStats)
