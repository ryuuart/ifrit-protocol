# SigilLoaderSource — the byte-source vocabulary (<sigilloader/Source.h>):
# Bytes, the ByteSource and Decoder concepts, and AnyByteSource, the
# type-erased source value. Header only and standard library only, so a
# decoder library can depend on it without inheriting the hub, libcurl,
# or any codec.
add_library(SigilLoaderSource INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilloader/Source.h
)
target_compile_features(SigilLoaderSource INTERFACE cxx_std_20)
target_include_directories(SigilLoaderSource INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/../include
)
