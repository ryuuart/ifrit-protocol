# The texture feature: an image and its sampling as a comparable leaf a
# material tree holds, the tools' texture-set vocabulary, the environment
# map a reflective surface is shaded from, and the atlas over a sheet.
# Links the core and SigilMedia (an asset is a source). No header here
# names a Skia type — the Skia executor is where a texture meets Skia —
# so Skia is private: the sources read and cut pixels with it. simdjson
# reads the sprite tools' JSON and stb packs the sheet, both private too.
# stb is a set of single-file headers, and its port publishes a find
# module and an include directory rather than a package — the contract
# the port's own usage states.
find_package(Stb REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMaterialTexture
  SOURCES Atlas.cpp EnvironmentMap.cpp Image.cpp Texture.cpp TextureSet.cpp
  HEADERS Atlas.h EnvironmentMap.h Image.h Texture.h TextureSet.h
  INCLUDE_PRIVATE ${Stb_INCLUDE_DIR}
  PUBLIC SigilMaterialCore SigilMediaCore Boost::container
  PRIVATE unofficial::skia::skia simdjson::simdjson)

# The cube-container cases decode a DDS and a KTX the test writes itself,
# through SigilMedia's routing surface and its fixture builders. The cases
# read pixels and shaders through the Skia executor, and the bevel's case
# lives here beside the environment it is shaded with.
sigil_test(material_test
  SOURCES test/TextureTest.cpp test/EnvironmentMapTest.cpp
          test/AtlasTest.cpp
  LIBRARIES SigilMaterialTexture SigilMaterialSkia SigilMediaImageDecode
  SUPPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../media/test)

sigil_bench(material_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilMaterialTexture SigilMaterialSkia)
