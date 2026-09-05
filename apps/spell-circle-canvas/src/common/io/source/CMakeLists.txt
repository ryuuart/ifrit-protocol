# The source leaf: the byte vocabulary in both directions — Bytes, the
# ByteSource, ResolvingByteSource and Decoder concepts, AnyByteSource
# (the type-erased source value), and the ByteSink concept with the
# local-filesystem write behind it. Header only and standard library
# only, so a decoder or an encoder can depend on it without inheriting
# the hub, libcurl, or any codec.
sigil_library(SigilIOSource
  HEADERS Sink.h Source.h)

# The concepts checked against a fixture source and a fixture decoder,
# with no hub, codec or disk in the binary.
sigil_test(io_source_test
  SOURCES test/SourceTest.cpp
  LIBRARIES SigilIOSource Boost::container)
