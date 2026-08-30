# The kit: presets — the reflective surfaces over a normal map and an
# environment, the girih panel, the gel and chrome colour tables, the text
# paints. Bodies are SkSL text; the kit links the pattern feature for the
# tile the panel bakes into and no renderer.
add_library(SigilMaterialKit STATIC
  LayerStyles.cpp
  Mask.cpp
  Patterns.cpp
  Surface.cpp
  Surfaces.cpp
  TextPaint.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/LayerStyles.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Mask.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Patterns.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Surface.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/Surfaces.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/kit/TextPaint.h
)
target_compile_features(SigilMaterialKit PUBLIC cxx_std_20)
target_include_directories(SigilMaterialKit PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialKit PUBLIC SigilMaterialPattern
  SigilMaterialColor)

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
