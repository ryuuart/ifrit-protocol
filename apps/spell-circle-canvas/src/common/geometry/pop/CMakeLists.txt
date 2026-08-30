# Point clouds with named attribute lanes: storage, generators,
# modifiers, instancing and the billboard rasterizer.
add_library(SigilGeometryPoints STATIC
  Billboards.cpp
  Cloud.cpp
  Generators.cpp
  Modifiers.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/points/Points.h
)
target_compile_features(SigilGeometryPoints PUBLIC cxx_std_20)
target_include_directories(SigilGeometryPoints PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryPoints PUBLIC SigilGeometryCurves)

add_executable(geometry_points_test test/PointsTest.cpp)
target_link_libraries(geometry_points_test PRIVATE SigilGeometryPoints
  GTest::gtest_main)
add_test(NAME geometry_points_test COMMAND geometry_points_test)
