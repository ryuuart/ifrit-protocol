# The connection: a feed read as values. It is the one feature here that
# stands on the hub rather than on bytes alone — a connection IS a door
# the hub opened, drained and read on the dispatch the hub runs — so it
# links SigilIOHub where every other feature of this library links the
# byte vocabulary and nothing else of io.
sigil_library(SigilDataConnection
  SOURCES Connection.cpp
  HEADERS Connection.h
  PUBLIC SigilDataDecode SigilIOHub)

# The cases put a connection on a real hub with a transport that has no
# socket under it, so what a message decodes to, which handler it
# reaches and what goes back down the wire are judged without a port.
# The schema cases read through the test schema the decode feature
# generates, whose include directory and dependency stand on data_test
# already: one schema serves the token and the door over it.
sigil_test(data_test
  SOURCES test/ConnectionTest.cpp test/SchemaTest.cpp
  LIBRARIES SigilDataConnection)
