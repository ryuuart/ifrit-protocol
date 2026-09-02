# The colour feature: the colour value that uploads as one float4, its
# packed spelling, and the OKLab round trip. The leaf of this library —
# the core's Params.h includes it, so it links nothing of this project's
# and no renderer; the standard library alone.
add_library(SigilMaterialColor STATIC
  Color.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/color/Color.h
)
target_compile_features(SigilMaterialColor PUBLIC cxx_std_20)
target_include_directories(SigilMaterialColor PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})

add_executable(material_color_test test/ColorTest.cpp)
target_link_libraries(material_color_test PRIVATE SigilMaterialColor
  GTest::gtest_main)
add_test(NAME material_color_test COMMAND material_color_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_color_bench bench/ColorBench.cpp)
target_link_libraries(material_color_bench PRIVATE SigilMaterialColor
  benchmark::benchmark)
add_dependencies(benches material_color_bench)
