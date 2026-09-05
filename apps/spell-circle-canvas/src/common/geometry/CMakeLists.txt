# SigilGeometry — 2D and 3D geometry over Skia, tiered by currency. The
# `path` tier is the 2D one: outlines resampled, addressed and operated
# on. The `mesh` tier is the 3D one: a triangle mesh, the camera that
# looks at it, the runtimes that draw it, splines, point clouds and
# model interchange. Each feature is its own static archive with its own
# tests and benchmarks, and links only the features beneath it, so a
# consumer that needs one tier links that tier alone. `SigilGeometry` is
# the umbrella: an interface over every feature, for a consumer of the
# whole library. Skia-only by contract: no compose kernel, no motion,
# no Qt.
sigil_library_root(SigilGeometry
  BRIEF "2D and 3D drawing over Skia: path operators, meshes, splines, point clouds")

add_subdirectory(path)
# The device feature stands both APIs on one GPU device; it comes BEFORE
# the tiers whose device executors stand on it.
add_subdirectory(device)
add_subdirectory(mesh)
add_subdirectory(kit)

sigil_library(SigilGeometry
  INTERFACE SigilGeometryPath SigilGeometryPathBlend SigilGeometryMesh
            SigilGeometryMeshCamera SigilGeometryMeshRender
            SigilGeometryMeshCurve SigilGeometryMeshPop
            SigilGeometryMeshCodec SigilGeometryKit SigilGeometryDevice)
