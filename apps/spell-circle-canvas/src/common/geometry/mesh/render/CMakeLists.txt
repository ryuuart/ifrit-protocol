# Drawing meshes and panels, and the Runtime seam a draw executes
# through. One executor ships here — everything on the CPU, emitting
# onto a Skia canvas — and a feature that owns a GPU device supplies its
# own as a value, so this target never learns about a device. The Runtime
# seam is a comparable erased value, which is SigilCoreComparable's shape.
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilGeometryMeshRender
  SOURCES Painter.cpp Runtime.cpp Shading.cpp
  HEADERS Painter.h Runtime.h Shading.h
  PUBLIC SigilGeometryMesh SigilGeometryMeshCamera SigilCoreComparable
  PRIVATE Boost::unordered)

sigil_test(geometry_test
  SOURCES test/PainterTest.cpp test/RuntimeTest.cpp test/ShadingTest.cpp
  LIBRARIES SigilGeometryMeshRender)

sigil_bench(geometry_bench
  SOURCES bench/RenderBench.cpp
  LIBRARIES SigilGeometryMeshRender SigilGeometryKit)

# …and the device executor of the same seam, its own target so that this
# one keeps its promise to know nothing about a device.
add_subdirectory(device)
