# Model interchange: one reader per format into the Mesh and Cloud
# currencies, and the PLY writer back out. The parsers are private
# plumbing — tinyobjloader for OBJ, header-only cgltf for glTF/GLB,
# Alembic (Ogawa only; the hdf5 feature stays off and legacy archives
# return nullopt); STL, PLY and .geo are parsed by hand. None of them
# reaches a public header.
add_library(SigilGeometryMeshCodec STATIC
  Alembic.cpp
  Geo.cpp
  Gltf.cpp
  Internal.h
  Model.cpp
  Obj.cpp
  PlyDecode.cpp
  PlyEncode.cpp
  Stl.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/codec/Decode.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/codec/Encode.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/codec/Model.h
)
target_compile_features(SigilGeometryMeshCodec PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMeshCodec PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
# workaround: the cgltf port installs headers and exports no config.
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
target_include_directories(SigilGeometryMeshCodec PRIVATE ${CGLTF_INCLUDE_DIR})
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Alembic CONFIG REQUIRED)
target_link_libraries(SigilGeometryMeshCodec
  PUBLIC SigilGeometryMesh SigilGeometryMeshPop
  PRIVATE tinyobjloader::tinyobjloader Alembic::Alembic)

# The decode tests write Alembic archives in memory with the O* API, so
# the binary needs the library directly, not just the codec's door; they
# also cook a pop chain over an imported cloud.
add_executable(geometry_mesh_codec_test
  test/DecodeTest.cpp
  test/EncodeTest.cpp
)
target_include_directories(geometry_mesh_codec_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../test)
target_link_libraries(geometry_mesh_codec_test PRIVATE SigilGeometryMeshCodec
  SigilGeometryMeshPop SigilGeometryKit GTest::gtest_main Alembic::Alembic)
add_test(NAME geometry_mesh_codec_test COMMAND geometry_mesh_codec_test)

add_executable(geometry_mesh_codec_bench bench/CodecBench.cpp)
target_link_libraries(geometry_mesh_codec_bench PRIVATE SigilGeometryMeshCodec
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_codec_bench)
