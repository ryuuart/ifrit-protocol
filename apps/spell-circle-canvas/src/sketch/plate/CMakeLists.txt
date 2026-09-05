# The plate feature: the headless sweep, the montage, and the comparison
# of two directories of plates. Every sketch is stepped to its
# declared moment and photographed, so a plate is a function of the
# declaration alone and never of how fast the machine ran. The IO byte
# writer a plate is saved through is read in the source alone.
#
# THE DEVICE LANE MAKES NO DEVICE. A `--gpu` sweep draws through the
# Graphite context the process's device already carries, which is why
# SigilGeometryDevice is on this line: a set is rendered by the runtime
# the host installed on that device, and a canvas is photographed on a
# surface allocated from the same one. A device of the plate's own would
# be a second set of textures neither could read out of the other.
sigil_library(SigilSketchPlate PIC
  SOURCES Compare.cpp Graphite.cpp Story.cpp Sweep.cpp
  HEADERS Compare.h FrameStats.h Graphite.h Story.h Sweep.h
  PUBLIC SigilSketchCore SigilMeasure SigilImageEncode SigilVideoCore
  PRIVATE SigilGeometryDevice SigilSkiaGraphite SigilIOSource
          SigilImageDecode SigilVideoEncode SigilWeaveFonts)

sigil_test(sketch_plate_test
  SOURCES test/CompareTest.cpp test/SweepTest.cpp
  LIBRARIES SigilSketchPlate SigilSketchCanvas SigilWeavePorts)

sigil_bench(sketch_plate_bench
  SOURCES bench/SweepBench.cpp
  LIBRARIES SigilSketchPlate SigilSketchCanvas SigilWeavePorts)
