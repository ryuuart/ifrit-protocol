# The Skia backend: the SkSL compiler the core's program cache runs, the
# builder that uploads resolved bytes into an SkRuntimeEffect, the leaf
# binding of a texture into a child slot, the draw of a material over a
# path, the one colour bridge between SkColor4f and this library's Color,
# Paint — the model as ONE Skia shader, with the gradients, the image and
# buffer sources, the SkSL leaf with its live uniforms and child slots,
# the blend stack, and the door onto a recipe instance — and Effect, the
# post-processing recipe over a layer a consumer has already rendered.
# Links the
# texture feature (and through it the core and Skia) and SigilMotion, for
# the animatable a live uniform and a bound pan hold.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

add_library(SigilMaterialSkia STATIC
  Draw.cpp
  Effect.cpp
  Paint.cpp
  ShaderSources.cpp
  ShaderSources.h
  SkiaCompiler.cpp
  ${_shaders}/PhosphorBloom.sksl
  ${_shaders}/ParamBlurMix.sksl
  ${_shaders}/Mix.sksl
  ${_shaders}/UnitRamp.sksl
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Color.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Draw.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Effect.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/Paint.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/skia/SkiaCompiler.h
)
target_compile_features(SigilMaterialSkia PUBLIC cxx_std_20)
target_include_directories(SigilMaterialSkia PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_compile_definitions(SigilMaterialSkia
  PRIVATE SIGIL_MATERIAL_SKIA_SHADER_DIR="${_shaders}")
target_link_libraries(SigilMaterialSkia
  PUBLIC SigilMaterialTexture SigilMaterialColor SigilMotionValues
         unofficial::skia::skia)
target_link_libraries(SigilMaterialSkia PRIVATE SigilIOHub)

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
