# The clocks: a stopwatch, the laps that tile one span between them, and
# the frame timer whose four marks feed the three lanes a render loop is
# judged by. Header-only over std::chrono; the lanes are the statistics
# library's rings, which is the one link out of here.
sigil_library(SigilMeasureTime
  HEADERS FrameTimer.h Laps.h Stopwatch.h
  INTERFACE SigilMeasureStats)

sigil_test(measure_test
  SOURCES test/TimeTest.cpp
  LIBRARIES SigilMeasureTime)
