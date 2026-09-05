# The Skia backend: the SkSL compiler the core's program cache runs, the
# builder that uploads resolved bytes into an SkRuntimeEffect, the leaf
# binding of a texture into a child slot, the draw of a material over a
# path, the one colour bridge between SkColor4f and this library's Color,
# Paint — the model as ONE Skia shader, with the gradients, the image and
# buffer sources, the SkSL leaf with its live uniforms and child slots,
# the blend stack, and the door onto a recipe instance — and Effect, the
# post-processing recipe over a layer a consumer has already rendered.
# Links the texture feature (and through it the core and Skia) and
# SigilMotion, for the animatable a live uniform and a bound pan hold.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialSkia
  SOURCES Draw.cpp Effect.cpp Paint.cpp ShaderSources.cpp ShaderSources.h
          SkiaCompiler.cpp
          ${_shaders}/PhosphorBloom.sksl ${_shaders}/ParamBlurMix.sksl
          ${_shaders}/Mix.sksl ${_shaders}/UnitRamp.sksl
  HEADERS Color.h Draw.h Effect.h Paint.h SkiaCompiler.h
  PUBLIC SigilMaterialTexture SigilMaterialColor SigilMotionValues
         unofficial::skia::skia
  PRIVATE SigilIOHub)
target_compile_definitions(SigilMaterialSkia
  PRIVATE SIGIL_MATERIAL_SKIA_SHADER_DIR="${_shaders}")

sigil_test(material_skia_test
  SOURCES test/SkiaTest.cpp
  LIBRARIES SigilMaterialSkia)

sigil_bench(material_skia_bench
  SOURCES bench/SkiaBench.cpp
  LIBRARIES SigilMaterialSkia)
