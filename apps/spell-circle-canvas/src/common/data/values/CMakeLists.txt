# THE FRIENDLY HEADER OVER A GENERATED ONE. `flatc --cpp` writes
# accessors that read a buffer in place: a string is a pointer that may
# be null, a vector is a pointer that may be null, and every one of them
# is only good while the bytes are. `sigil_schema_values` reads the same
# schema with FlatBuffers' own parser and writes a second header beside
# the first, in which the schema's tables are plain structs of
# std::string, std::vector, std::optional and std::variant. It stands ON
# the generated header and never replaces it: every reading goes through
# a generated accessor and every writing through a generated builder.
#
# Values.h is what those generated headers include — the readings and
# the writings that have no schema in them, and the one place a whole
# buffer is verified. It is a header and nothing else, so this target is
# INTERFACE; it names the decode feature because a generated header
# converts the root's JSON form through a Schema.
find_package(flatbuffers CONFIG REQUIRED)

sigil_library(SigilDataValues
  HEADERS Values.h
  INTERFACE SigilDataDecode flatbuffers::flatbuffers)

# The generator itself: one translation unit over the FlatBuffers
# parser, which is the only thing it needs. It is a tool the build runs,
# not a library anyone links, so it stands beside the binaries rather
# than among the tests.
add_executable(sigil_schema_values SchemaValues.cpp)
target_link_libraries(sigil_schema_values PRIVATE flatbuffers::flatbuffers)
set_target_properties(sigil_schema_values PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tools)

set(VALUES_TEST_OUT ${CMAKE_CURRENT_BINARY_DIR}/schema)

# THE CASES' OWN SCHEMA, carrying every shape a value has to say: a
# nested struct, a table field that may be absent, a vector of tables, a
# required string, an enum, a deprecated field and a union of three.
# Both headers come from the one schema in one rule, since the friendly
# one includes the generated one by name and neither is any use without
# the other.
set(VALUES_TEST_SCHEMA ${CMAKE_CURRENT_LIST_DIR}/test/values_test.fbs)
add_custom_command(
  OUTPUT ${VALUES_TEST_OUT}/values_test_generated.h
         ${VALUES_TEST_OUT}/values_test_bfbs_generated.h
         ${VALUES_TEST_OUT}/values_test_values.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${VALUES_TEST_OUT}
  COMMAND flatbuffers::flatc --cpp -b --schema --bfbs-gen-embed
          -o ${VALUES_TEST_OUT} ${VALUES_TEST_SCHEMA}
  COMMAND sigil_schema_values ${VALUES_TEST_SCHEMA} -o ${VALUES_TEST_OUT}
  DEPENDS ${VALUES_TEST_SCHEMA} sigil_schema_values
  COMMENT "Generating values_test_values.h from the cases' own schema"
  VERBATIM)

# THE DECODE FEATURE'S TEST SCHEMA, read as values as well. Its
# generated header and its include directory are the decode feature's
# already, so only the friendly header is written here: one schema read
# both ways is how a case says that the same sheet comes back.
set(VALUES_SHEET_SCHEMA ${SIGIL_LIBRARY_ROOT}/decode/test/flatbuffer_test.fbs)
add_custom_command(
  OUTPUT ${VALUES_TEST_OUT}/flatbuffer_test_values.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${VALUES_TEST_OUT}
  COMMAND sigil_schema_values ${VALUES_SHEET_SCHEMA} -o ${VALUES_TEST_OUT}
  DEPENDS ${VALUES_SHEET_SCHEMA} sigil_schema_values
  COMMENT "Generating flatbuffer_test_values.h from the decode test schema"
  VERBATIM)

# The rules enter the build graph through a target in their own
# directory; the test binary is made in the first feature's, so it
# depends on this.
add_custom_target(SigilDataValuesTestSchema
  DEPENDS ${VALUES_TEST_OUT}/values_test_values.h
          ${VALUES_TEST_OUT}/flatbuffer_test_values.h)

# The cases read a value out of bytes and write it back, so what the
# generator wrote is judged against the wire rather than against itself:
# a value that survives the round trip, a union through each
# alternative, an absent table field, a vector of tables, the root's
# JSON form both ways, and bytes that are not this schema at all.
sigil_test(data_test
  SOURCES test/ValuesTest.cpp
  LIBRARIES SigilDataValues SigilDataDecode)
add_dependencies(data_test SigilDataValuesTestSchema)
target_include_directories(data_test PRIVATE ${VALUES_TEST_OUT})
