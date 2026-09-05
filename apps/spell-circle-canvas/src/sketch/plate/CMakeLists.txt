# The plate feature: the headless sweep, the montage, and the comparison
# of two directories of plates. Every sketch is stepped to its
# declared moment and photographed, so a plate is a function of the
# declaration alone and never of how fast the machine ran. The IO byte
# writer a plate is saved through is read in the source alone.
sigil_library(SigilSketchPlate PIC
  SOURCES Compare.cpp Story.cpp Sweep.cpp
  HEADERS Compare.h FrameStats.h Graphite.h Story.h Sweep.h
  PUBLIC SigilSketchCore SigilMeasure SigilImageEncode SigilVideoCore
  PRIVATE SigilIOSource SigilImageDecode SigilVideoEncode SigilWeaveFonts)

# The device lane: a process-owned device and queue with a Graphite
# context over them, since a run with no window shares nobody's.
if(APPLE)
  target_sources(SigilSketchPlate PRIVATE GraphiteMetal.mm)
  target_link_libraries(SigilSketchPlate PRIVATE SigilSkia)
  target_compile_definitions(SigilSketchPlate PRIVATE SIGILSKETCH_HEADLESS_GPU)
endif()

sigil_test(sketch_plate_test
  SOURCES test/CompareTest.cpp test/SweepTest.cpp
  LIBRARIES SigilSketchPlate SigilSketchCanvas SigilWeavePorts)

sigil_bench(sketch_plate_bench
  SOURCES bench/SweepBench.cpp
  LIBRARIES SigilSketchPlate SigilSketchCanvas SigilWeavePorts)
