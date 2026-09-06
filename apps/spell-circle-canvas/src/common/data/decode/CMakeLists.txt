# The decoders: delimiter-separated text and JSON into a Table, and a
# JSON document as a value of its own.
#
# It speaks io's BYTE vocabulary and nothing else of io: a decoder takes
# Bytes and a name hint, and registerDecoders() is a template over the
# hub, so this target never links the hub, its cache or its codecs. The
# JSON parser is private and reaches no public header.
find_package(simdjson CONFIG REQUIRED)
sigil_library(SigilDataDecode
  SOURCES Csv.cpp Json.cpp Decoders.cpp
  HEADERS Csv.h Json.h Decoders.h
  PUBLIC SigilDataTable SigilIOSource
  PRIVATE simdjson::simdjson)

# The hub is named here and not in the library: the cases put the
# decoders on a real hub and read a real file back through it, which is
# the whole of what registerDecoders() promises.
sigil_test(data_test
  SOURCES test/DecodeTest.cpp
  LIBRARIES SigilDataDecode SigilIOHub)

sigil_bench(data_bench
  SOURCES bench/DecodeBench.cpp
  LIBRARIES SigilDataDecode)
