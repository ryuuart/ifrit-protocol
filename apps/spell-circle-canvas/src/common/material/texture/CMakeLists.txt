# The texture feature: an image and its sampling as a comparable leaf a
# material tree holds, the tools' texture-set vocabulary, the environment
# map and the bevel producer a reflective surface is shaded from, and the atlas
# over a sheet. Links the core and SigilImage (an asset is a source);
# simdjson reads the sprite tools' JSON and stb packs the sheet, both
# private: no consumer sees either.
find_package(simdjson CONFIG REQUIRED)
# workaround: the stb port ships a find module rather than a config
# package, so it is found by module and reached by include directory.
find_package(Stb REQUIRED)
sigil_library(SigilMaterialTexture
  SOURCES Atlas.cpp EnvironmentMap.cpp Surface.cpp Texture.cpp TextureSet.cpp
  HEADERS Atlas.h EnvironmentMap.h ShaderLeaf.h Surface.h Texture.h
          TextureSet.h
  INCLUDE_PRIVATE ${Stb_INCLUDE_DIR}
  PUBLIC SigilMaterialCore SigilImageAsset unofficial::skia::skia
         Boost::container
  PRIVATE simdjson::simdjson)

# The cube-container cases decode a DDS and a KTX the test writes itself,
# through SigilImage's routing surface and its fixture builders.
sigil_test(material_texture_test
  SOURCES test/TextureTest.cpp
  LIBRARIES SigilMaterialTexture SigilImageDecode
  SUPPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../image/test)

sigil_bench(material_texture_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilMaterialTexture)
