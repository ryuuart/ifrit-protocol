# The core feature: what a sketch is, what it declares about its canvas,
# what it reaches for, and the seam that says which runtime draws it.
#
# It links the resource hub (a sketch reaches for files), the comparable
# erasure the kind seam is built on, the instruments every runtime times
# itself with, and the camera whose orbit the viewpoint seam is spelled
# in — and nothing that draws: a consumer that only wants to know what
# sketches exist links this alone.
#
# The crash reporter is here rather than beside either host because BOTH
# have a guest: the live host calls into a dylib it just loaded, and the
# sweep calls into a hundred sketches in one process, and a fault inside
# one is a fault inside the host either way.
sigil_library(SigilSketchCore PIC
  SOURCES Assets.cpp Crash.cpp Device.cpp Registry.cpp Sources.cpp
  HEADERS Assets.h CanvasSpec.h Crash.h Device.h Kind.h Registry.h Session.h
          Sources.h
  PUBLIC SigilCoreComparable SigilGeometryMeshCamera SigilImageAsset
         SigilIO SigilVideoDecode Boost::container)

sigil_test(sketch_core_test
  SOURCES test/AssetsTest.cpp test/CrashTest.cpp test/DeviceTest.cpp
          test/RegistryTest.cpp test/SourcesTest.cpp
  LIBRARIES SigilSketchCore SigilVideoEncode)

sigil_bench(sketch_core_bench
  SOURCES bench/RegistryBench.cpp
  LIBRARIES SigilSketchCore)
