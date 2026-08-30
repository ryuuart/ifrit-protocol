# Skia's 3D put to work: the camera, the painter-order software
# rasterizer and the perspective panels.
add_library(SigilGeometrySpace STATIC
  Space.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/space/Space.h
)
target_compile_features(SigilGeometrySpace PUBLIC cxx_std_20)
target_include_directories(SigilGeometrySpace PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometrySpace PUBLIC SigilGeometryMesh)

add_executable(geometry_space_test test/SpaceTest.cpp)
target_include_directories(geometry_space_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(geometry_space_test PRIVATE SigilGeometrySpace
  GTest::gtest_main)
add_test(NAME geometry_space_test COMMAND geometry_space_test)
