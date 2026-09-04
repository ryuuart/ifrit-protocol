# The source leaf: the byte vocabulary in both directions — Bytes, the
# ByteSource, ResolvingByteSource and Decoder concepts, AnyByteSource
# (the type-erased source value), and the ByteSink concept with the
# local-filesystem write behind it. Header only and standard library
# only, so a decoder or an encoder can depend on it without inheriting
# the hub, libcurl, or any codec.
add_library(SigilIOSource INTERFACE
  ${SIGIL_IO_INCLUDE_DIR}/sigilio/source/Sink.h
  ${SIGIL_IO_INCLUDE_DIR}/sigilio/source/Source.h
)
target_compile_features(SigilIOSource INTERFACE cxx_std_20)
target_include_directories(SigilIOSource INTERFACE
  ${SIGIL_IO_INCLUDE_DIR})

# The concepts checked against a fixture source and a fixture decoder,
# with no hub, codec or disk in the binary.
add_executable(io_source_test test/SourceTest.cpp)
target_include_directories(io_source_test PRIVATE ${SIGIL_TEST_SUPPORT_DIR})
target_link_libraries(io_source_test PRIVATE SigilIOSource
  Boost::container GTest::gtest_main)
add_test(NAME io_source_test COMMAND io_source_test)
