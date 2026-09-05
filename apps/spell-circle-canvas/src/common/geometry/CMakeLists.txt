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
#
# Every public header lives under one include root,
# include/sigilgeometry/, and the directories under it nest the way the
# targets do, so a feature adds the root and spells its own directory.

set(SIGIL_GEOMETRY_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(path)
# The device feature needs Graphite on the shared device, since the whole
# point of it is that both APIs draw on one; it is absent otherwise, and
# so are the device executors that stand on it. It comes BEFORE the tiers
# that ask whether it exists.
if(TARGET SigilSkiaGraphite)
  add_subdirectory(device)
else()
  message(STATUS
    "SigilGeometryDevice skipped: needs SPELLCIRCLE_ENABLE_SKIA_CANVAS "
    "(SigilSkiaGraphite)")
endif()
add_subdirectory(mesh)
add_subdirectory(kit)

add_library(SigilGeometry INTERFACE)
target_link_libraries(SigilGeometry INTERFACE
  SigilGeometryPath
  SigilGeometryPathBlend
  SigilGeometryMesh
  SigilGeometryMeshCamera
  SigilGeometryMeshRender
  SigilGeometryMeshCurve
  SigilGeometryMeshPop
  SigilGeometryMeshCodec
  SigilGeometryKit
)
if(TARGET SigilGeometryDevice)
  target_link_libraries(SigilGeometry INTERFACE SigilGeometryDevice)
endif()

# The header tree installs as one unit beside the exported path leaf.
install(DIRECTORY include/sigilgeometry DESTINATION include)

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SigilGeometry
  BRIEF "2D and 3D drawing over Skia: path operators, meshes, splines, point clouds"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
