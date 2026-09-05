# The live feature: the reload engine. Qt-free on purpose — it watches a
# source file, rebuilds it into a versioned dylib with the compiler flags
# the build captured, dlopens the result and swaps the running session,
# keeping the previous one alive while a build is broken. The host's
# header publishes the samples and stopwatch a build is timed with, so
# SigilMeasure is PUBLIC; the IO byte writer a rebuilt source is saved
# through is read in the source alone.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilSketchLive PIC
  SOURCES Host.cpp Residency.cpp
  HEADERS Host.h Residency.h
  PUBLIC SigilSketchCore SigilImageEncode SigilMotionClock SigilMeasure
         Boost::container
  PRIVATE SigilIOSource SigilCoreSchedule)

sigil_test(sketch_live_test
  SOURCES test/HostTest.cpp test/ResidencyTest.cpp
  LIBRARIES SigilSketchLive SigilSketchCanvas)

sigil_bench(sketch_live_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketchLive SigilSketchCanvas)
