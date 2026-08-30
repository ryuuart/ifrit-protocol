# The source leaf: the byte-source vocabulary — Bytes, the ByteSource,
# ResolvingByteSource and Decoder concepts, and AnyByteSource, the
# type-erased source value. Header only and standard library only, so a
# decoder library can depend on it without inheriting the hub, libcurl,
# or any codec.
add_library(SigilLoaderSource INTERFACE
  ${SIGIL_LOADER_INCLUDE_DIR}/sigilloader/source/Source.h
)
target_compile_features(SigilLoaderSource INTERFACE cxx_std_20)
target_include_directories(SigilLoaderSource INTERFACE
  ${SIGIL_LOADER_INCLUDE_DIR})

# The concepts checked against a fixture source and a fixture decoder,
# with no hub, codec or disk in the binary.
add_executable(loader_source_test test/SourceTest.cpp)
target_link_libraries(loader_source_test PRIVATE SigilLoaderSource
  GTest::gtest_main)
add_test(NAME loader_source_test COMMAND loader_source_test)
