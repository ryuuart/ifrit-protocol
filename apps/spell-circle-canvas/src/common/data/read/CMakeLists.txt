# The one-line readers: a hub's bytes read as a document, a table or a
# store's answer, and the call that puts this library's decoders on a
# hub. It is the one feature besides the connection that links the hub
# itself, because reading a URI is asking one.
sigil_library(SigilDataRead
  SOURCES Read.cpp
  HEADERS Read.h
  PUBLIC SigilDataDecode SigilDataTable
  PRIVATE SigilDataQuery SigilIOHub)

sigil_test(data_test
  SOURCES test/ReadTest.cpp
  LIBRARIES SigilDataRead SigilDataQuery SigilIOHub)
