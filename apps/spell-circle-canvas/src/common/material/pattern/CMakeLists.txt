# The pattern feature: the tile mechanism — one bake, a mapping, an
# explicit reseed — and the stock tile generators over it. Links the
# texture feature (a tile is a repeating texture) and the geometry leaf
# for the seeded hash the speckle scatters with.
add_library(SigilMaterialPattern STATIC
  Patterns.cpp
  Tile.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/pattern/Patterns.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/pattern/Tile.h
)
target_compile_features(SigilMaterialPattern PUBLIC cxx_std_20)
target_include_directories(SigilMaterialPattern PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialPattern PUBLIC SigilMaterialTexture)

add_executable(material_pattern_test test/PatternTest.cpp)
target_link_libraries(material_pattern_test PRIVATE SigilMaterialPattern
  GTest::gtest_main)
add_test(NAME material_pattern_test COMMAND material_pattern_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_pattern_bench bench/PatternBench.cpp)
target_link_libraries(material_pattern_bench PRIVATE SigilMaterialPattern
  benchmark::benchmark)
add_dependencies(benches material_pattern_bench)
