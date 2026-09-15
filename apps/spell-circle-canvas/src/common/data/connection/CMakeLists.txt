# The connection: a feed read as values. It is the one feature here that
# stands on the hub rather than on bytes alone — a connection IS a door
# the hub opened, drained and read on the dispatch the hub runs — so it
# links SigilIOHub where every other feature of this library links the
# byte vocabulary and nothing else of io.
#
# SigilDataValues is the typed door's: latest<Value>() reads the newest
# bytes through the trait a generated value header specializes, which is
# declared there. It is a header and the trait is a template, so nothing
# of it reaches this library's translation unit.
sigil_library(SigilDataConnection
  SOURCES Connection.cpp
  HEADERS Connection.h
  PUBLIC SigilDataDecode SigilDataValues SigilIOHub)

# The cases put a connection on a real hub with a transport that has no
# socket under it, so what a message decodes to, which handler it
# reaches and what goes back down the wire are judged without a port.
# The schema cases read through the test schema the decode feature
# generates, and the typed cases read the same schema's value header,
# which the values feature writes; both include directories and both
# rules stand on data_test already, so one schema serves the token, the
# door over it and the value it hands out.
sigil_test(data_test
  SOURCES test/ConnectionTest.cpp test/SchemaTest.cpp test/TypedTest.cpp
  LIBRARIES SigilDataConnection)
