# SigilLoader — the runtime resource system: mounted res:// URIs,
# caching, metadata probes, hot reload. Access only: all image format
# knowledge lives in SigilImage (sigilimage/Decode.h), which the hub
# delegates to.
add_library(SigilLoader STATIC
  Hub.cpp
  include/sigilloader/Loader.h
)
target_compile_features(SigilLoader PUBLIC cxx_std_20)
target_include_directories(SigilLoader PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilLoader PUBLIC SigilImage)

# The test writes its own EXR fixtures, so it needs OIIO directly (the
# library itself does not).
find_package(OpenImageIO CONFIG QUIET)

add_executable(loader_test
  test/LoaderTest.cpp
)
target_link_libraries(loader_test PRIVATE SigilLoader GTest::gtest_main)
if(OpenImageIO_FOUND)
  target_link_libraries(loader_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(loader_test PRIVATE SIGILLOADER_HAS_OIIO)
endif()
add_test(NAME loader_test COMMAND loader_test)
