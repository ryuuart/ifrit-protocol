# Model interchange: one reader per format into the Mesh and Cloud
# currencies, and the PLY writer back out. The parsers are private
# plumbing — tinyobjloader for OBJ, header-only cgltf for glTF/GLB,
# Alembic (Ogawa only; the hdf5 feature stays off and legacy archives
# return nullopt); STL, PLY and .geo are parsed by hand.
add_library(SigilGeometryCodec STATIC
  Alembic.cpp
  Geo.cpp
  Gltf.cpp
  Internal.h
  Model.cpp
  Obj.cpp
  PlyDecode.cpp
  PlyEncode.cpp
  Stl.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/codec/Decode.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/codec/Encode.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/codec/Model.h
)
target_compile_features(SigilGeometryCodec PUBLIC cxx_std_20)
target_include_directories(SigilGeometryCodec PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
target_include_directories(SigilGeometryCodec PRIVATE ${CGLTF_INCLUDE_DIR})
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Alembic CONFIG REQUIRED)
target_link_libraries(SigilGeometryCodec
  PUBLIC SigilGeometryMesh SigilGeometryPop
  PRIVATE tinyobjloader::tinyobjloader Alembic::Alembic)

# The decode tests write Alembic archives in memory with the O* API, so
# the binary needs the library directly, not just the codec's door; they
# also cook a pop chain over an imported cloud.
add_executable(geometry_codec_test
  test/DecodeTest.cpp
  test/EncodeTest.cpp
)
target_include_directories(geometry_codec_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(geometry_codec_test PRIVATE SigilGeometryCodec
  SigilGeometryPop GTest::gtest_main Alembic::Alembic)
add_test(NAME geometry_codec_test COMMAND geometry_codec_test)

add_executable(geometry_codec_bench bench/CodecBench.cpp)
target_link_libraries(geometry_codec_bench PRIVATE SigilGeometryCodec
  benchmark::benchmark)
add_dependencies(benches geometry_codec_bench)
