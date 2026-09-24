# THE PROTOCOL'S GENERATOR: the definition's reflected schema read, and
# each domain's agent seam, its C++ client and the description the
# Python generator reads written out of it. A file per subject: the
# model every writer walks, what the writers share, the agent header,
# the client header, the list of every table, the description, and the
# tool that runs them. It is
# a tool the build runs, not a library anyone links, so it stands beside
# the binaries rather than among the tests.
find_package(flatbuffers CONFIG REQUIRED)

add_executable(sigil_protocol
  Protocol.cpp ProtocolModel.cpp ProtocolModel.h ProtocolWriting.cpp
  ProtocolWriting.h ProtocolOutputs.h ProtocolAgents.cpp ProtocolClients.cpp
  ProtocolTables.cpp ProtocolDescription.cpp)
target_link_libraries(sigil_protocol PRIVATE flatbuffers::flatbuffers)
set_target_properties(sigil_protocol PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tools)
