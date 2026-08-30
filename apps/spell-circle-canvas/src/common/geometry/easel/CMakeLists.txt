# The artist façade: header-only, so the target is an interface over the
# features it reads like a sentence.
add_library(SigilGeometryEasel INTERFACE)
target_sources(SigilGeometryEasel INTERFACE
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/easel/Easel.h
)
target_include_directories(SigilGeometryEasel INTERFACE ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryEasel INTERFACE
  SigilGeometryBlend
  SigilGeometryCurves
  SigilGeometryMaterial
  SigilGeometryMesh
  SigilGeometryPath
  SigilGeometryPoints
  SigilGeometrySpace
)

add_executable(geometry_easel_test test/EaselTest.cpp)
target_link_libraries(geometry_easel_test PRIVATE SigilGeometryEasel
  GTest::gtest_main)
add_test(NAME geometry_easel_test COMMAND geometry_easel_test)
