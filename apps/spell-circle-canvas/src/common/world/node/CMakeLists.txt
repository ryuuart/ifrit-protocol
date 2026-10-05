# The node feature: one node of a 3D scene as a comparable copy-on-write
# value, in its five kinds — group, null, mesh, light and camera — over
# one description underneath; the verb families they share, the
# transform lanes, collections, the light values and the generator seam.
# Nothing here cooks, draws or retains: it links the vocabularies it is
# written in, the reconciler's kernels, and SigilMaterial's surface
# program, which a filled material is lowered into once at the verb — and
# nothing that owns a device.
find_package(glm CONFIG REQUIRED)
sigil_library(SigilWorldNode
  SOURCES Collection.cpp Equal.cpp Geometry.cpp Lanes.cpp Light.cpp Verbs.cpp
  HEADERS Camera.h Collection.h Group.h Light.h Mesh.h Null.h Transform.h
          Verbs.h ../advanced/Geometry.h ../advanced/Lanes.h
          ../advanced/Light.h ../advanced/Node.h
  PUBLIC SigilCoreComparable SigilCoreCompute SigilCoreReconcile
         SigilCoreCache SigilGeometryMesh SigilGeometryMeshCamera
         SigilGeometryMeshCurve SigilGeometryMeshPop SigilMaterialCore
         SigilMaterialTexture SigilMaterialSurface SigilMotionValues
         SigilMotionSchedule glm::glm)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
sigil_test(world_test
  SOURCES test/NodeTest.cpp test/LightTest.cpp test/CollectionTest.cpp
  LIBRARIES SigilWorldNode)

sigil_bench(world_bench
  SOURCES bench/NodeBench.cpp bench/LightBench.cpp
  LIBRARIES SigilWorldNode)
