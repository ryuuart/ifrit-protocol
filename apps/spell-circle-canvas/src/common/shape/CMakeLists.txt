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
  Geometry.cpp
  Materials.cpp
  Mesh.cpp
  Space.cpp
  include/sigilshape/Blend.h
  include/sigilshape/Geometry.h
  include/sigilshape/Materials.h
  include/sigilshape/Mesh.h
  include/sigilshape/Space.h
)

target_compile_features(SigilShape PUBLIC cxx_std_20)

target_include_directories(SigilShape PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# earcut.hpp is PRIVATE plumbing (cap triangulation in Mesh.cpp); the
# public headers expose only Skia types.
find_path(EARCUT_INCLUDE_DIR mapbox/earcut.hpp REQUIRED)
target_include_directories(SigilShape PRIVATE ${EARCUT_INCLUDE_DIR})

target_link_libraries(SigilShape PUBLIC unofficial::skia::skia)

add_executable(shape_test
  test/ShapeTest.cpp
)

target_link_libraries(shape_test PRIVATE SigilShape GTest::gtest_main)

add_test(NAME shape_test COMMAND shape_test)

# Headless PNG panels of the SigilShape catalog: blend studies, material
# swatches, procedural meshes in perspective. Mirrors compose_demo:
#   shape_demo [outdir]            (default shape_demo_out)
add_executable(shape_demo
  examples/shape_demo.cpp
)

# SigilLoader feeds the optional HDRI-environment panel (fetch_assets).
target_link_libraries(shape_demo PRIVATE SigilShape SigilLoader)
