# The texture feature: an image and its sampling as a comparable leaf a
# material tree holds, the tools' texture-set vocabulary, the environment
# and bevel producers a reflective surface is shaded from, and the atlas
# over a sheet. Links the core and SigilImage (an asset is a source);
# simdjson reads the sprite tools' JSON and stb packs the sheet, both
# private: no consumer sees either.
find_package(simdjson CONFIG REQUIRED)
# workaround: the stb port ships a find module rather than a config
# package, so it is found by module and reached by include directory.
find_package(Stb REQUIRED)
add_library(SigilMaterialTexture STATIC
  Atlas.cpp
  Surface.cpp
  Texture.cpp
  TextureSet.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/texture/Atlas.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/texture/ShaderLeaf.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/texture/Surface.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/texture/Texture.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/texture/TextureSet.h
)
target_compile_features(SigilMaterialTexture PUBLIC cxx_std_20)
target_include_directories(SigilMaterialTexture
  PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR}
  PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(SigilMaterialTexture
  PUBLIC SigilMaterialCore SigilImageAsset unofficial::skia::skia
  PRIVATE simdjson::simdjson)

add_executable(material_texture_test test/TextureTest.cpp)
target_link_libraries(material_texture_test PRIVATE SigilMaterialTexture
  GTest::gtest_main)
add_test(NAME material_texture_test COMMAND material_texture_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(material_texture_bench bench/TextureBench.cpp)
target_link_libraries(material_texture_bench PRIVATE SigilMaterialTexture
  benchmark::benchmark)
add_dependencies(benches material_texture_bench)
