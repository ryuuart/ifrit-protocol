# SigilLoader — the runtime resource system: mounted res:// URIs,
# caching, metadata probes, hot reload. Access only: the hub is a
# ByteSource (SigilLoaderSource, added below) and every typed view is a
# registered decoder run over the bytes it fetched. All image format
# knowledge lives in SigilImageDecode (sigilimage/Decode.h), whose
# decoders the hub registers by default.
add_subdirectory(source)

add_library(SigilLoader STATIC
  Hub.cpp
  include/sigilloader/Loader.h
)
target_compile_features(SigilLoader PUBLIC cxx_std_20)
target_include_directories(SigilLoader PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilLoader PUBLIC SigilLoaderSource SigilImageDecode)

# libcurl backs the http(s):// URIs (fetch + on-disk cache); pure
# transport, so it stays a private implementation detail.
find_package(CURL CONFIG REQUIRED)
target_link_libraries(SigilLoader PRIVATE CURL::libcurl)

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

# The hub benchmark (Google Benchmark; use a Release build): cache hits,
# typed-view dispatch and URI resolution, the disk kept out of the timed
# loop. Not a test: it builds through the `benches` target and runs
# through scripts/bench_ledger.py.
add_executable(loader_bench bench/LoaderBench.cpp)
target_link_libraries(loader_bench PRIVATE SigilLoader benchmark::benchmark)
add_dependencies(benches loader_bench)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilLoader
  BRIEF "Runtime resource hub: resources addressed by URI, cached, and reloaded when they change"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
