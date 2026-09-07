# The registry, the kind seam, the crash reporter and where a sketch
# stands on disk: the part of SigilSketch that draws nothing. The
# reporter is covered here rather than beside either host because BOTH
# have a guest — the live host calls into a dylib it just loaded, the
# sweep into a hundred sketches in one process — and a fault inside one
# is a fault inside the host either way.
sigil_test(sketch_test
  SOURCES test/AssetsTest.cpp test/CrashTest.cpp test/DeviceTest.cpp
          test/RegistryTest.cpp test/SourcesTest.cpp
  LIBRARIES SigilSketch SigilVideoEncode)

sigil_bench(sketch_bench
  SOURCES bench/RegistryBench.cpp
  LIBRARIES SigilSketch)
