# The point-operator chain language and its CPU reference executor.
add_library(SigilGeometryPop STATIC
  Cook.cpp
  Fields.cpp
  Sinks.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/pop/Pop.h
)
target_compile_features(SigilGeometryPop PUBLIC cxx_std_20)
target_include_directories(SigilGeometryPop PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryPop PUBLIC SigilGeometryCurves
  SigilGeometryPoints)

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
add_executable(geometry_pop_test test/PopTest.cpp)
target_include_directories(geometry_pop_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(geometry_pop_test PRIVATE SigilGeometryPop
  SigilGeometryCodec GTest::gtest_main)
add_test(NAME geometry_pop_test COMMAND geometry_pop_test)

add_executable(geometry_pop_bench bench/PopBench.cpp)
target_link_libraries(geometry_pop_bench PRIVATE SigilGeometryPop
  benchmark::benchmark)
add_dependencies(benches geometry_pop_bench)
