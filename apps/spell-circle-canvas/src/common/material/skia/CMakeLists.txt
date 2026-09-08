# The Skia backend: the SkSL compiler the core's program cache runs, the
# builder that uploads resolved bytes into an SkRuntimeEffect, the leaf
# binding of a texture into a child slot, the draw of a material over a
# path, the one colour bridge between SkColor4f and this library's Color,
# Paint — the model as ONE Skia shader, with the gradients, the image and
# buffer sources, the SkSL leaf with its live uniforms and child slots,
# the blend stack, and the door onto a recipe instance — Effect, the
# post-processing recipe over a layer a consumer has already rendered, and
# the four crossings out of the colour leaf: two that turn a ramp's stops
# into a gradient, and two that carry a palette as a lookup table — an
# N x 1 image and the nearest-sampled material a child slot takes.
# Links the texture feature (and through it the core and Skia) and
# SigilMotion, for the animatable a live uniform and a bound pan hold.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMaterialSkia
  SOURCES Draw.cpp Effect.cpp EffectBloom.cpp EffectBlurs.cpp
          EffectDoors.cpp Paint.cpp PaintBlend.cpp PaintBuild.cpp
          PaintDetail.cpp PaintResolve.cpp PaintSources.cpp
          PaintUniforms.cpp Palette.cpp Ramp.cpp SkiaCompiler.cpp
  HEADERS Color.h Draw.h Effect.h Paint.h PaintDetail.h PixelBuffer.h
          Palette.h Ramp.h SkiaCompiler.h
  PUBLIC SigilMaterialTexture SigilMaterialColor SigilMotionValues
         unofficial::skia::skia)
sigil_shader_sources(SigilMaterialSkia
  DIR ${_shaders} NAMESPACE sigil::material::skia)

# The shader directory is named for the case that asks whether the
# embedded table holds every file in it.
sigil_test(material_test
  SOURCES test/SkiaCompilerTest.cpp test/SkiaColorTest.cpp
          test/SkiaPaintTest.cpp test/SkiaEffectTest.cpp
          test/SkiaRampTest.cpp test/PaletteTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SKIA_SHADER_DIR="${_shaders}"
  LIBRARIES SigilMaterialSkia)

sigil_bench(material_bench
  SOURCES bench/SkiaBench.cpp
  LIBRARIES SigilMaterialSkia)
