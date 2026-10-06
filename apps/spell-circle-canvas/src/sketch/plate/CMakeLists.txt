# The headless renderer: the sweep that steps every sketch to its
# declared moment and photographs it, the montage, the comparison of two
# directories of plates, and the thumbnail store a browser fills on
# demand. A plate is a function of the declaration alone and never of
# how fast the machine ran.
#
# A sweep over a scene3d sketch draws it on the device, so the cases that
# photograph one link the device executor and the one device a test process
# brings up, and skip where the machine has no Vulkan runtime.
sigil_test(sketch_test
  SOURCES test/CompareTest.cpp test/SweepTest.cpp test/ThumbnailQueueTest.cpp
          test/ThumbnailWriterTest.cpp test/ThumbnailsTest.cpp
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilWeavePorts SigilWorldDiligent
  SUITES DeviceSweep
  LABELS gpu)

sigil_bench(sketch_bench
  SOURCES bench/SweepBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)
