# The signed-distance surfaces: three recipes, every style dial a uniform.
# Links the core alone.
add_library(SigilMaterialSdf STATIC
  Sdf.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/sdf/Sdf.h
)
target_compile_features(SigilMaterialSdf PUBLIC cxx_std_20)
target_include_directories(SigilMaterialSdf PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialSdf PUBLIC SigilMaterialCore
  SigilMaterialColor)

# The test shades, so it links the Skia backend beside the feature.
add_executable(material_sdf_test test/SdfTest.cpp)
target_link_libraries(material_sdf_test PRIVATE SigilMaterialSdf
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_sdf_test COMMAND material_sdf_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_sdf_bench bench/SdfBench.cpp)
target_link_libraries(material_sdf_bench PRIVATE SigilMaterialSdf
  SigilMaterialSkia benchmark::benchmark)
add_dependencies(benches material_sdf_bench)
