# The field feature: surfaces evaluated per pixel — the halftone ramp,
# Perlin noise through a leaf, luminance grain, the ripple. Links the
# texture feature for the leaf seam.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
add_library(SigilMaterialField STATIC
  Field.cpp
  ${_shaders}/CrtOverlay.sksl
  ${_shaders}/Grain.sksl
  ${_shaders}/HalftoneRamp.sksl
  ${_shaders}/Noise.sksl
  ${_shaders}/Ripple.sksl
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/field/Field.h
)
target_compile_features(SigilMaterialField PUBLIC cxx_std_20)
target_include_directories(SigilMaterialField PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_compile_definitions(SigilMaterialField
  PRIVATE SIGIL_MATERIAL_FIELD_SHADER_DIR="${_shaders}")
target_link_libraries(SigilMaterialField PUBLIC SigilMaterialTexture
  SigilMaterialColor)
target_link_libraries(SigilMaterialField PRIVATE SigilIOHub)

add_executable(material_field_test test/FieldTest.cpp)
target_link_libraries(material_field_test PRIVATE SigilMaterialField
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_field_test COMMAND material_field_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_field_bench bench/FieldBench.cpp)
target_link_libraries(material_field_bench PRIVATE SigilMaterialField
  SigilMaterialSkia benchmark::benchmark)
add_dependencies(benches material_field_bench)
