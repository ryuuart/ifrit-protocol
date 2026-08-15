# SigilShape — the higher-level drawing vocabulary over Skia: shape
# interpolation (the Illustrator blend tool), procedural 3D geometry
# (extrude/revolve/parametric meshes shared with SigilWorld), Skia-3D
# rendering (SkM44 perspective + SkVertices painter pipeline), and
# literal materials (gold foil / chrome / glass as SkSL runtime effects
# over derived normal maps and environment lookups). Skia-only by
# contract: no compose kernel, no motion, no Qt — SigilCompose adapts
# downward, never the reverse.
add_library(SigilShape STATIC
  Blend.cpp
  Curves.cpp
  Geometry.cpp
  Import.cpp
  Pop.cpp
  Materials.cpp
  Mesh.cpp
  Ops.cpp
  Points.cpp
  Save.cpp
  Space.cpp
  include/sigilshape/Blend.h
  include/sigilshape/Curves.h
  include/sigilshape/Easel.h
  include/sigilshape/Geometry.h
  include/sigilshape/Import.h
  include/sigilshape/Materials.h
  include/sigilshape/Mesh.h
  include/sigilshape/Ops.h
  include/sigilshape/Points.h
  include/sigilshape/Pop.h
  include/sigilshape/Save.h
  include/sigilshape/Space.h
)

target_compile_features(SigilShape PUBLIC cxx_std_20)

target_include_directories(SigilShape PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# earcut.hpp is PRIVATE plumbing (cap triangulation in Mesh.cpp); the
# public headers expose only Skia and glm types. The model-import parsers
# (Import.cpp) are the same shape of detail: tinyobjloader for OBJ,
# header-only cgltf for glTF/GLB, STL by hand.
find_path(EARCUT_INCLUDE_DIR mapbox/earcut.hpp REQUIRED)
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
target_include_directories(SigilShape PRIVATE ${EARCUT_INCLUDE_DIR}
  ${CGLTF_INCLUDE_DIR})
find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(SigilShape PRIVATE tinyobjloader::tinyobjloader)

# Alembic (Ogawa) is the fifth import door — the vfx cache format.
# Ogawa-only: the hdf5 feature stays off, legacy archives return nullopt.
find_package(Alembic CONFIG REQUIRED)
target_link_libraries(SigilShape PRIVATE Alembic::Alembic)

# glm is the 3D currency (vec3/vec4/mat4 in Mesh/Curves/Points/Pop/
# Import and downstream in SigilWorld); Skia stays the 2D one (paths,
# paint, textures, the Space painter bridge).
find_package(glm CONFIG REQUIRED)
target_link_libraries(SigilShape PUBLIC unofficial::skia::skia glm::glm)

add_executable(shape_test
  test/ShapeTest.cpp
)

# The test writes Alembic archives in memory with the O* API, so it
# needs the headers/libs directly, not just SigilShape's import door.
target_link_libraries(shape_test PRIVATE SigilShape GTest::gtest_main
  Alembic::Alembic)

add_test(NAME shape_test COMMAND shape_test)

# Headless PNG panels of the SigilShape catalog: blend studies, material
# swatches, procedural meshes in perspective. Mirrors compose_demo:
#   shape_demo [outdir]            (default shape_demo_out)
add_executable(shape_demo
  examples/shape_demo.cpp
)

# SigilLoader feeds the optional HDRI-environment panel (fetch_assets).
# SigilCompose + SigilWeave author the yarn-marquee panel's canvas —
# demo-only deps; the shape library itself stays Skia-only.
target_link_libraries(shape_demo PRIVATE SigilShape SigilLoader
  SigilCompose SigilWeave SigilWeaveShaders SigilWeavePorts
  SigilWeaveKit)

# The CPU pop executor and the .geo reader under load (Google Benchmark;
# use a Release build):
#   ./build/bin/Release/shape_bench
add_executable(shape_bench bench/ShapeBench.cpp)
target_link_libraries(shape_bench PRIVATE SigilShape benchmark::benchmark)
