# The source leaf: the byte vocabulary in both directions — Bytes, the
# ByteSource, ResolvingByteSource and Decoder concepts, AnyByteSource
# (the type-erased source value), and the ByteSink concept with the
# local-filesystem write behind it, and the two places only the platform
# can name: where the running binary stands and where a process may leave
# throwaway files. Standard library plus the one platform call that
# answers an executable's own path, so a decoder or an encoder can depend
# on it without inheriting the hub, libcurl, or any codec.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilIOSource
  SOURCES Places.cpp
  HEADERS Places.h Sink.h Source.h)

# The concepts checked against a fixture source and a fixture decoder,
# with no hub, codec or disk in the binary.
sigil_test(io_test
  SOURCES test/SourceTest.cpp
  LIBRARIES SigilIOSource Boost::container)
