# SigilGeometry — 2D and 3D geometry over Skia: path resampling and
# operators, shape interpolation (the Illustrator blend tool), procedural
# 3D geometry (extrude/revolve/parametric meshes shared with SigilWorld),
# Skia-3D rendering (SkM44 perspective + SkVertices painter pipeline), and
# literal materials (gold foil / chrome / glass as SkSL runtime effects
# over derived normal maps and environment lookups). Skia-only by
# contract: no compose kernel, no motion, no Qt — SigilCompose adapts
# downward, never the reverse.
# The path leaf: polylines, contours addressed by arc length, noise and
# the numeric routines under them. Skia and glm only — no mesh, no import,
# no material — so a text engine or a drawable-component library can walk
# an outline without linking the 3D half of geometry or its importers.
add_library(SigilGeometryPath STATIC
  Contour.cpp
  Noise.cpp
  Ops.cpp
  Polyline.cpp
  include/sigilgeometry/Contour.h
  include/sigilgeometry/Noise.h
  include/sigilgeometry/Numeric.h
  include/sigilgeometry/Ops.h
  include/sigilgeometry/Polyline.h
  include/sigilgeometry/Skia.h
)
target_compile_features(SigilGeometryPath PUBLIC cxx_std_20)
target_include_directories(SigilGeometryPath PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
find_package(glm CONFIG REQUIRED)
target_link_libraries(SigilGeometryPath PUBLIC unofficial::skia::skia glm::glm)

# The leaf is a public dependency of the text engine, whose install rules
# export it; an exported target's dependencies must themselves sit in an
# export set, so the leaf ships under its own.
install(TARGETS SigilGeometryPath
  EXPORT SigilGeometryPathTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/sigilgeometry DESTINATION include)
install(EXPORT SigilGeometryPathTargets
  NAMESPACE sigil::geometry::
  DESTINATION share/sigilgeometry
)

add_library(SigilGeometry STATIC
  Blend.cpp
  Curves.cpp
  Import.cpp
  Pop.cpp
  Materials.cpp
  Mesh.cpp
  Points.cpp
  Save.cpp
  Space.cpp
  include/sigilgeometry/Blend.h
  include/sigilgeometry/Curves.h
  include/sigilgeometry/Easel.h
  include/sigilgeometry/Import.h
  include/sigilgeometry/Materials.h
  include/sigilgeometry/Mesh.h
  include/sigilgeometry/Points.h
  include/sigilgeometry/Pop.h
  include/sigilgeometry/Save.h
  include/sigilgeometry/Space.h
)
target_compile_features(SigilGeometry PUBLIC cxx_std_20)

target_include_directories(SigilGeometry PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# earcut.hpp is PRIVATE plumbing (cap triangulation in Mesh.cpp); the
# public headers expose only Skia and glm types. The model-import parsers
# (Import.cpp) are the same shape of detail: tinyobjloader for OBJ,
# header-only cgltf for glTF/GLB, STL by hand.
find_path(EARCUT_INCLUDE_DIR mapbox/earcut.hpp REQUIRED)
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
target_include_directories(SigilGeometry PRIVATE ${EARCUT_INCLUDE_DIR}
  ${CGLTF_INCLUDE_DIR})
find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(SigilGeometry PRIVATE tinyobjloader::tinyobjloader)

# Alembic (Ogawa) is the fifth import door — the vfx cache format.
# Ogawa-only: the hdf5 feature stays off, legacy archives return nullopt.
find_package(Alembic CONFIG REQUIRED)
target_link_libraries(SigilGeometry PRIVATE Alembic::Alembic)

# glm is the 3D currency (vec3/vec4/mat4 in Mesh/Curves/Points/Pop/
# Import and downstream in SigilWorld); Skia stays the 2D one (paths,
# paint, textures, the Space painter bridge).
target_link_libraries(SigilGeometry PUBLIC SigilGeometryPath)

add_executable(geometry_path_test
  test/PathTest.cpp
)
target_link_libraries(geometry_path_test PRIVATE SigilGeometryPath
  GTest::gtest_main)
add_test(NAME geometry_path_test COMMAND geometry_path_test)

# One test binary per subsystem, each a single translation unit, so an edit
# to one subsystem recompiles one small file. Helpers that more than one
# binary reads live in test/support/GeometrySupport.h.
function(geometry_suite_test name source)
  add_executable(${name} test/${source})
  target_include_directories(${name} PRIVATE test)
  target_link_libraries(${name} PRIVATE SigilGeometry GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()
geometry_suite_test(geometry_blend_test BlendTest.cpp)
geometry_suite_test(geometry_mesh_test MeshTest.cpp)
geometry_suite_test(geometry_curves_test CurvesTest.cpp)
geometry_suite_test(geometry_points_test PointsTest.cpp)
geometry_suite_test(geometry_pop_test PopTest.cpp)
geometry_suite_test(geometry_easel_test EaselTest.cpp)

# The import tests write Alembic archives in memory with the O* API, so this
# one binary needs the headers and libraries directly, not just
# SigilGeometry's import door.
add_executable(geometry_import_test test/ImportTest.cpp)
target_include_directories(geometry_import_test PRIVATE test)
target_link_libraries(geometry_import_test PRIVATE SigilGeometry
  GTest::gtest_main Alembic::Alembic)
add_test(NAME geometry_import_test COMMAND geometry_import_test)

# Headless PNG panels of the SigilGeometry catalog: blend studies, material
# swatches, procedural meshes in perspective. Mirrors compose_demo:
#   geometry_demo [outdir]            (default geometry_demo_out)
add_executable(geometry_demo
  examples/geometry_demo.cpp
)

# SigilLoader feeds the optional HDRI-environment panel (fetch_assets).
# SigilCompose + SigilWeave author the yarn-marquee panel's canvas —
# demo-only deps; the shape library itself stays Skia-only.
target_link_libraries(geometry_demo PRIVATE SigilGeometry SigilLoader
  SigilCompose SigilWeave SigilWeaveShaders SigilWeavePorts
  SigilWeaveKit)

# The CPU pop executor and the .geo reader under load (Google Benchmark;
# use a Release build):
#   ./build/bin/Release/geometry_bench
add_executable(geometry_bench bench/GeometryBench.cpp)
target_link_libraries(geometry_bench PRIVATE SigilGeometry benchmark::benchmark)

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
