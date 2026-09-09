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
            SigilGeometryMeshRenderDevice
            SigilGeometryMeshCurve SigilGeometryMeshPop
            SigilGeometryMeshPopDevice
            SigilGeometryMeshCodec SigilGeometryKit SigilGeometryDevice
            SigilGeometryDeviceResidency)

# The umbrella's own case: it links the whole interface and includes
# Geometry.h, so a public header that stops compiling beside its
# siblings — a name two features spell the same way — is a build failure
# here rather than in whichever consumer took both.
sigil_test(geometry_test
  SOURCES test/UmbrellaTest.cpp
  LIBRARIES SigilGeometry)

# Every API name the prose spells, compiled against the headers that own
# it. SigilCore is scanned beside this library's own root for the chance
# source, the noise field and the PCG words the path tier agrees with,
# and SigilSkia for the offscreen surface the device walkthrough paints
# into; the hardware and surface headers are named as preludes because
# this library's own only forward-declare what they hold.
#
# The device residency values are named on purpose without being
# probeable: their headers are the residency feature's PRIVATE ones, as
# the same passage says, so no consumer include path carries them.
sigil_doc_probes(geometry
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include
  PRELUDES "<sigilcore/hardware/GpuDevice.h>"
           "<sigilcore/hardware/Handle.h>"
           "<sigilskia/graphite/OffscreenSurface.h>"
  EXCLUDE "glm::vec3::length=glm's own static member, named to warn against it: the vector maths library's headers are not scanned"
          "device::MeshResidency=declared in the device residency feature's private headers, which no consumer include path carries"
          "device::MeshVertex=declared in the device residency feature's private headers, which no consumer include path carries"
          "device::TextureResidency=declared in the device residency feature's private headers, which no consumer include path carries"
          "device::Resources=declared in the device residency feature's private headers, which no consumer include path carries"
          "device::PipelineCache=declared in the device residency feature's private headers, which no consumer include path carries"
  FLOORS 100,12,12,0,0
  LIBRARIES SigilGeometry SigilCoreHardware SigilSkiaGraphite)
