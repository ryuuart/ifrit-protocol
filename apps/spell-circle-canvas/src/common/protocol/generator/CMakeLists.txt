# THE PROTOCOL'S GENERATOR: the definition's reflected schema read, and
# each domain's agent seam, its C++ client and the description the
# Python generator reads written out of it. A file per subject: the
# model every writer walks, what the writers share, the agent header,
# the client header, the list of every table, the description, and the
# tool that runs them. It is a tool the build runs, not a library anyone
# links, so it stands beside the binaries rather than among the tests.
find_package(flatbuffers CONFIG REQUIRED)

# The model, and every rule of the definition's shape it refuses, apart
# from the tool, so the library's test binary can hand it a definition
# that breaks each rule.
add_library(sigil_protocol_model STATIC ProtocolModel.cpp ProtocolModel.h)
target_include_directories(sigil_protocol_model
  PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(sigil_protocol_model PUBLIC flatbuffers::flatbuffers)

add_executable(sigil_protocol
  Protocol.cpp ProtocolWriting.cpp ProtocolWriting.h ProtocolOutputs.h
  ProtocolAgents.cpp ProtocolClients.cpp ProtocolTables.cpp
  ProtocolDescription.cpp)
target_link_libraries(sigil_protocol PRIVATE sigil_protocol_model)
set_target_properties(sigil_protocol PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tools)

# Every rule the model refuses, each broken by a definition of a few
# lines parsed in the test itself.
sigil_test(protocol_test
  SOURCES test/ModelTest.cpp
  LIBRARIES sigil_protocol_model
  LABELS protocol)
