# SigilComposeTexture — a compose scene as a material::Texture.
#
# The one feature here that owns a SURFACE: it keeps a composer and paints
# it into a raster surface, or into a texture on a GPU device through
# Graphite when a host hands it one. What it produces is an ordinary
# material texture value, which is why nothing downstream branches on
# "is this a scene": a 3D surface, a pattern or an atlas samples it the
# way it samples any other image.
#
# Links the kernel and SigilMaterial's texture feature, because both
# appear in its header, the Graphite context and the hardware device the
# GPU path stands on, and the motion clock privately, because the
# composer it keeps is stepped by one.
sigil_library(SigilComposeTexture
  SOURCES Texture.cpp
  HEADERS Texture.h
  PUBLIC SigilComposeCore SigilMaterialTexture SigilSkiaGraphite
         SigilCoreHardware
  PRIVATE SigilMotionClock)
target_compile_definitions(SigilComposeTexture PUBLIC
  SIGILCOMPOSE_TEXTURE_DEVICE=1)

# The raster half runs anywhere; the device half skips without a GPU, so
# the whole binary carries the label rather than pretending to cover it.
sigil_test(compose_texture_test
  SOURCES test/ComposeTextureTest.cpp
  LIBRARIES SigilComposeTexture SigilComposeTesting SigilWeavePorts
  LABELS gpu)

sigil_bench(compose_texture_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilComposeTexture SigilWeavePorts)
