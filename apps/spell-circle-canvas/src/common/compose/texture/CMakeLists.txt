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
# appear in its header, and the motion clock privately, because the
# composer it keeps is stepped by one. The device feature is added only
# when this build carries one; without it the raster surface stands and
# useDevice() answers false.
add_library(SigilComposeTexture STATIC
  Texture.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/texture/Texture.h
)
target_compile_features(SigilComposeTexture PUBLIC cxx_std_20)
target_include_directories(SigilComposeTexture PUBLIC
  ${SIGIL_COMPOSE_INCLUDE_DIR})
target_link_libraries(SigilComposeTexture
  PUBLIC SigilComposeCore SigilMaterialTexture
  PRIVATE SigilMotionClock)
if(TARGET SigilSkiaGraphite)
  target_link_libraries(SigilComposeTexture PUBLIC SigilSkiaGraphite
    SigilCoreHardware)
  target_compile_definitions(SigilComposeTexture PUBLIC
    SIGILCOMPOSE_TEXTURE_DEVICE=1)
endif()

_compose_test_binary(compose_texture_test
  LIBRARIES SigilComposeTexture
  SOURCES
    test/ComposeTextureTest.cpp
)

_compose_bench(compose_texture_bench
  LIBRARIES SigilComposeTexture
  SOURCES
    bench/TextureBench.cpp
)
