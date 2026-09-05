# The element feature: one node of a 3D scene as a comparable
# copy-on-write value — the verbs, the transform lanes, the geometry
# slot, tags, selectors and the generator seam. Nothing here cooks, draws
# or retains: it links the vocabularies it is written in and the
# reconciler's kernels, and nothing that owns a device.
sigil_library(SigilWorldElement
  SOURCES Element.cpp Equal.cpp Geometry.cpp Lanes.cpp Selector.cpp
          Transform.cpp
  HEADERS Element.h Environment.h Geometry.h Lanes.h Node.h Selector.h
          Transform.h
  PUBLIC SigilCoreComparable SigilCoreCompute SigilCoreReconcile
         SigilCoreCache SigilGeometryMesh SigilGeometryMeshCamera
         SigilGeometryMeshCurve SigilGeometryMeshPop SigilMaterialCore
         SigilMaterialTexture SigilMotionValues SigilMotionSchedule
         SigilWorldLight)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
# The emitter values are here too: this target links the light feature,
# so a binary over the emitters alone would draw no boundary — the
# description and the emitters it carries are one closure.
sigil_test(world_test
  SOURCES test/ElementTest.cpp test/LightTest.cpp test/SelectorTest.cpp
  LIBRARIES SigilWorldElement)

sigil_bench(world_bench
  SOURCES bench/ElementBench.cpp
  LIBRARIES SigilWorldElement)
