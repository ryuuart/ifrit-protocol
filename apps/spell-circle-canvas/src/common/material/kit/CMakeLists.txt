# The kit: stock recipes — the reflective surfaces over a normal map and
# an environment. Bodies are SkSL text; the kit links the texture feature
# for the slots it fills and no renderer.
add_library(SigilMaterialKit STATIC
  Surfaces.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Surfaces.h
)
target_compile_features(SigilMaterialKit PUBLIC cxx_std_20)
target_include_directories(SigilMaterialKit PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialKit PUBLIC SigilMaterialTexture)

# The test draws, so it links the Skia backend beside the kit.
add_executable(material_kit_test test/KitTest.cpp)
target_link_libraries(material_kit_test PRIVATE SigilMaterialKit
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_kit_test COMMAND material_kit_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_kit_bench bench/KitBench.cpp)
target_link_libraries(material_kit_bench PRIVATE SigilMaterialKit
  SigilMaterialSkia benchmark::benchmark)
add_dependencies(benches material_kit_bench)
