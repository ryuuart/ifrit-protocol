# SigilGeometry — 2D and 3D geometry over Skia, as one feature library per
# directory. Each feature is its own static archive with its own tests and
# benchmarks, and links only the features beneath it, so a consumer that
# needs one tier links that tier alone. `SigilGeometry` is the umbrella:
# an interface over every feature, for a consumer of the whole library.
# Skia-only by contract: no compose kernel, no motion, no Qt.
#
# Every public header lives under one include root, include/sigilgeometry/
# <feature>/, so a feature adds the root and spells its own directory.

set(SIGIL_GEOMETRY_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(path)
add_subdirectory(blend)
add_subdirectory(mesh)
add_subdirectory(space)
add_subdirectory(curves)
add_subdirectory(points)
add_subdirectory(pop)
add_subdirectory(codec)
add_subdirectory(material)
add_subdirectory(easel)

add_library(SigilGeometry INTERFACE)
target_link_libraries(SigilGeometry INTERFACE
  SigilGeometryPath
  SigilGeometryBlend
  SigilGeometryMesh
  SigilGeometrySpace
  SigilGeometryCurves
  SigilGeometryPoints
  SigilGeometryPop
  SigilGeometryCodec
  SigilGeometryMaterial
  SigilGeometryEasel
)

# The header tree installs as one unit beside the exported path leaf.
install(DIRECTORY include/sigilgeometry DESTINATION include)

# Headless PNG panels of the SigilGeometry catalog: blend studies, material
# swatches, procedural meshes in perspective. Mirrors compose_demo:
#   geometry_demo [outdir]            (default geometry_demo_out)
add_executable(geometry_demo
  examples/geometry_demo.cpp
)

# SigilLoader feeds the optional HDRI-environment panel (fetch_assets).
# SigilComposeCore + SigilWeave author the yarn-marquee panel's canvas —
# demo-only deps; the library itself stays Skia-only.
target_link_libraries(geometry_demo PRIVATE SigilGeometry SigilLoader
  SigilComposeCore SigilWeave SigilWeaveShaders SigilWeavePorts
  SigilWeaveKit)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilGeometry
  BRIEF "2D and 3D drawing over Skia: path operators, meshes, splines, point clouds"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
