# SigilLoader — the runtime resource system: mounted res:// URIs over
# pluggable decode backends. SigilImage (Skia codecs) covers the web
# image formats and animation; OpenImageIO, when present, extends
# decoding and metadata to EXR (with layer/channel selection), PSD,
# TIFF, HDR, and the rest of its format roster. Blob/text resources,
# probe() metadata, and hot reload (poll) round out the hub.
add_library(SigilLoader STATIC
  Hub.cpp
  Decode.cpp
  include/sigilloader/Loader.h
)
target_compile_features(SigilLoader PUBLIC cxx_std_20)
target_include_directories(SigilLoader PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilLoader PUBLIC SigilImage)

find_package(OpenImageIO CONFIG QUIET)
if(OpenImageIO_FOUND)
  target_link_libraries(SigilLoader PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(SigilLoader PRIVATE SIGILLOADER_HAS_OIIO)
  set(SIGILLOADER_TEST_OIIO SIGILLOADER_HAS_OIIO)
  message(STATUS "SigilLoader: OpenImageIO backend enabled")
else()
  set(SIGILLOADER_TEST_OIIO "")
  message(WARNING
    "SigilLoader: OpenImageIO not found — EXR/PSD/TIFF loading disabled")
endif()

add_executable(loader_test
  test/LoaderTest.cpp
)
target_link_libraries(loader_test PRIVATE SigilLoader GTest::gtest_main)
if(OpenImageIO_FOUND)
  # The test writes its own EXR fixtures through OIIO.
  target_link_libraries(loader_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(loader_test PRIVATE ${SIGILLOADER_TEST_OIIO})
endif()
add_test(NAME loader_test COMMAND loader_test)
