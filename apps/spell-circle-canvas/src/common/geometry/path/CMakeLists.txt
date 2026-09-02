# The path leaf: polylines, contours addressed by arc length, noise, the
# numeric routines under them, where a run of n items falls on a ring or
# in a grid, the path operators, the two comparable seams a mark is
# deviated and widened through (Shaper and Profile) with the band those
# rails cut, where a set of paths cross, and the two coordinate systems a
# figure is measured in (the polar Frame and the unit-map Grid). Skia, glm and the
# standard-library mixers under SigilCoreCompute — no mesh, no import, no
# material — so a text engine or a drawable-component library can walk an
# outline, or place a run of things, without linking the 3D half of
# geometry or its importers.
add_library(SigilGeometryPath STATIC
  Band.cpp
  Contour.cpp
  Crossings.cpp
  Edges.cpp
  Noise.cpp
  Ops.cpp
  Polyline.cpp
  Pose.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Arrange.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Band.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Contour.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Crossings.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Edges.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Frame.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Noise.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Numeric.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Ops.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Polyline.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Pose.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Profile.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Shaper.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/Skia.h
)
target_compile_features(SigilGeometryPath PUBLIC cxx_std_20)
target_include_directories(SigilGeometryPath PUBLIC
  $<BUILD_INTERFACE:${SIGIL_GEOMETRY_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
find_package(glm CONFIG REQUIRED)
# Noise.h names the seeded mixers, which live one library down so that a
# shader's CPU twin and a resource key reach the same bodies.
target_link_libraries(SigilGeometryPath PUBLIC unofficial::skia::skia glm::glm
  SigilCoreCompute)

# The leaf is a public dependency of the text engine, whose install rules
# export it; an exported target's dependencies must themselves sit in an
# export set, so the leaf ships under its own.
install(TARGETS SigilGeometryPath
  EXPORT SigilGeometryPathTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(EXPORT SigilGeometryPathTargets
  NAMESPACE sigil::geometry::
  DESTINATION share/sigilgeometry
)

add_executable(geometry_path_test test/PathTest.cpp)
target_link_libraries(geometry_path_test PRIVATE SigilGeometryPath
  GTest::gtest_main)
add_test(NAME geometry_path_test COMMAND geometry_path_test)

# Google Benchmark, one binary per feature so each links only what it
# measures; built through the `benches` target and run through
# scripts/bench_ledger.py from a Release build.
add_executable(geometry_path_bench bench/PathBench.cpp)
target_link_libraries(geometry_path_bench PRIVATE SigilGeometryPath
  benchmark::benchmark)
add_dependencies(benches geometry_path_bench)

add_subdirectory(blend)
