# The decoders: delimiter-separated text and JSON into a Table, and a
# JSON document as a value of its own.
#
# It speaks io's BYTE vocabulary and nothing else of io: a decoder takes
# Bytes and a name hint, and registerDecoders() is a template over the
# hub, so this target never links the hub, its cache or its codecs. The
# JSON parser is private and reaches no public header.
find_package(simdjson CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
sigil_library(SigilDataDecode
  SOURCES Csv.cpp Json.cpp Decoders.cpp
  HEADERS Csv.h Json.h Decoders.h FlatBuffer.h
  PUBLIC SigilDataTable SigilIOSource flatbuffers::flatbuffers
  PRIVATE simdjson::simdjson)

# The FlatBuffer decoder's cases read a schema of their own, generated the way
# a sketch's is: the header beside its binary schema, which the header
# embeds so the JSON form converts with no schema file read.
set(FLATBUFFER_TEST_SCHEMA ${CMAKE_CURRENT_LIST_DIR}/test/flatbuffer_test.fbs)
set(FLATBUFFER_TEST_OUT ${CMAKE_CURRENT_BINARY_DIR}/schema)
add_custom_command(
  OUTPUT ${FLATBUFFER_TEST_OUT}/flatbuffer_test_generated.h
         ${FLATBUFFER_TEST_OUT}/flatbuffer_test_bfbs_generated.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${FLATBUFFER_TEST_OUT}
  COMMAND flatbuffers::flatc --cpp -b --schema --bfbs-gen-embed
          -o ${FLATBUFFER_TEST_OUT} ${FLATBUFFER_TEST_SCHEMA}
  DEPENDS ${FLATBUFFER_TEST_SCHEMA}
  COMMENT "Generating flatbuffer_test_generated.h from the test schema"
  VERBATIM)
# The rule enters the build graph through a target in its own directory;
# the test binary is made in the first feature's, so it depends on this.
add_custom_target(SigilDataFlatBufferTestSchema
  DEPENDS ${FLATBUFFER_TEST_OUT}/flatbuffer_test_generated.h)

# The hub is named here and not in the library: the cases put the
# decoders on a real hub and read a real file back through it, which is
# the whole of what registerDecoders() and registerFlatBuffer() promise.
sigil_test(data_test
  SOURCES test/DecodeTest.cpp test/FlatBufferTest.cpp
  LIBRARIES SigilDataDecode SigilIOHub)
add_dependencies(data_test SigilDataFlatBufferTestSchema)
target_include_directories(data_test PRIVATE ${FLATBUFFER_TEST_OUT})

sigil_bench(data_bench
  SOURCES bench/DecodeBench.cpp
  LIBRARIES SigilDataDecode)
