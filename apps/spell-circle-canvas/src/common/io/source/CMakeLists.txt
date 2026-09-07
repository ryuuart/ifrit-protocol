# The source leaf: the byte vocabulary in both directions — Bytes, the
# ByteSource, ResolvingByteSource, Decoder and Probable concepts,
# AnyByteSource (the type-erased source value), and the ByteSink concept
# with the local-filesystem write behind it; the archive source, which is
# one file with files inside it answered as any other source answers; and
# the two places only the platform can name: where the running binary
# stands and where a process may leave throwaway files. Standard library
# plus the one platform call that answers an executable's own path and
# the zip reader behind the archive source, both PRIVATE, so a decoder or
# an encoder can depend on this without inheriting the hub, libcurl, or
# any codec.
find_package(minizip-ng CONFIG REQUIRED)
sigil_library(SigilIOSource
  SOURCES Archive.cpp Places.cpp
  HEADERS Archive.h Places.h Sink.h Source.h
  PRIVATE MINIZIP::minizip-ng)

# The concepts checked against a fixture source and a fixture decoder,
# with no hub, codec or disk in the binary. The flat map is the test's
# own, which is why Boost is found here and not above it: the library
# links no Boost at all.
find_package(boost_container CONFIG REQUIRED)
sigil_test(io_test
  SOURCES test/ArchiveTest.cpp test/SourceTest.cpp
  LIBRARIES SigilIOSource Boost::container)
