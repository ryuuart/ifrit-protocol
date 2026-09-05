# Model interchange: one reader per format into the Mesh and Cloud
# currencies, and the PLY and .geo writers back out. The parsers are private
# plumbing — tinyobjloader for OBJ, header-only cgltf for glTF/GLB,
# Alembic (Ogawa only; the hdf5 feature stays off and legacy archives
# return nullopt), simdjson for the JSON a `.geo` is; STL and PLY are
# parsed by hand. None of them reaches a public header.
#
# cgltf is a single-file header, and its port installs that header and
# nothing else — no package, no find module — so a consumer locates the
# header and puts its directory on the include path.
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Alembic CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
sigil_library(SigilGeometryMeshCodec
  SOURCES Alembic.cpp Geo.cpp GeoEncode.cpp Gltf.cpp Internal.h Model.cpp
          Obj.cpp PlyDecode.cpp PlyEncode.cpp Stl.cpp
  HEADERS Decode.h Encode.h Model.h
  INCLUDE_PRIVATE ${CGLTF_INCLUDE_DIR}
  PUBLIC SigilGeometryMesh SigilGeometryMeshPop Boost::container
  PRIVATE tinyobjloader::tinyobjloader Alembic::Alembic Boost::unordered
          simdjson::simdjson)

# The decode tests write Alembic archives in memory with the O* API, so
# the binary needs the library directly, not just the codec's door; they
# also cook a pop chain over an imported cloud.
# One file per format, plus the Model operations over whatever reader made
# it and the writers' return leg.
sigil_test(geometry_test
  SOURCES test/ObjTest.cpp test/GltfTest.cpp test/StlTest.cpp
          test/PlyTest.cpp test/AlembicTest.cpp test/GeoTest.cpp
          test/ModelTest.cpp test/EncodeTest.cpp
  LIBRARIES SigilGeometryMeshCodec SigilGeometryMeshPop SigilGeometryKit
            Alembic::Alembic)

sigil_bench(geometry_bench
  SOURCES bench/CodecBench.cpp
  LIBRARIES SigilGeometryMeshCodec)
