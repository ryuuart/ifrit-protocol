# The Skia backend: the SkSL compiler the core's program cache runs, the
# builder that uploads resolved bytes into an SkRuntimeEffect, the leaf
# binding of a texture into a child slot, the draw of a material over a
# path, and the one colour bridge between SkColor4f and this library's
# Color. Links the texture feature (and through it the core and Skia),
# nothing else.
add_library(SigilMaterialSkia STATIC
  Draw.cpp
  SkiaCompiler.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Color.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Draw.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/SkiaCompiler.h
)
target_compile_features(SigilMaterialSkia PUBLIC cxx_std_20)
target_include_directories(SigilMaterialSkia PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialSkia
  PUBLIC SigilMaterialTexture unofficial::skia::skia)

add_executable(material_skia_test test/SkiaTest.cpp)
target_link_libraries(material_skia_test PRIVATE SigilMaterialSkia
  GTest::gtest_main)
add_test(NAME material_skia_test COMMAND material_skia_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(material_skia_bench bench/SkiaBench.cpp)
target_link_libraries(material_skia_bench PRIVATE SigilMaterialSkia
  benchmark::benchmark)
add_dependencies(benches material_skia_bench)
