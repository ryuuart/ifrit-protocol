# Reflective materials as Skia runtime effects over derived normal maps
# and environment lookups. Skia alone — nothing else in the library.
add_library(SigilGeometryMaterial STATIC
  Bevel.cpp
  Environment.cpp
  Shaders.cpp
  Sksl.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/material/Materials.h
)
target_compile_features(SigilGeometryMaterial PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMaterial PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryMaterial PUBLIC unofficial::skia::skia)

add_executable(geometry_material_test test/MaterialTest.cpp)
target_link_libraries(geometry_material_test PRIVATE SigilGeometryMaterial
  GTest::gtest_main)
add_test(NAME geometry_material_test COMMAND geometry_material_test)
