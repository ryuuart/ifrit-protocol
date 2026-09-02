# The hub: mounts and resolution, the entry cache and its poll, network
# fetch behind the disk cache, and the decoder registry — one
# translation unit each, joined by the private Fetch.h seam.
add_library(SigilLoaderHub STATIC
  Cache.cpp
  Decoders.cpp
  Mounts.cpp
  Network.cpp
  Fetch.h
  ${SIGIL_LOADER_INCLUDE_DIR}/sigilloader/hub/Hub.h
  ${SIGIL_LOADER_INCLUDE_DIR}/sigilloader/hub/Network.h
)
target_compile_features(SigilLoaderHub PUBLIC cxx_std_20)
target_include_directories(SigilLoaderHub PUBLIC ${SIGIL_LOADER_INCLUDE_DIR})
target_link_libraries(SigilLoaderHub PUBLIC SigilLoaderSource SigilImageDecode)

# libcurl backs the http(s):// URIs (fetch + on-disk cache); pure
# transport, so it stays a private implementation detail.
find_package(CURL CONFIG REQUIRED)
target_link_libraries(SigilLoaderHub PRIVATE CURL::libcurl)

# The test builds its own PNG fixtures through the image encoder and
# writes its own EXR ones through OIIO directly — the library itself
# encodes nothing.
find_package(OpenImageIO CONFIG QUIET)

add_executable(loader_hub_test test/HubTest.cpp)
target_include_directories(loader_hub_test PRIVATE ${SIGIL_TEST_SUPPORT_DIR})
target_link_libraries(loader_hub_test PRIVATE SigilLoaderHub SigilImageEncode
  GTest::gtest_main)
if(OpenImageIO_FOUND)
  target_link_libraries(loader_hub_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(loader_hub_test PRIVATE SIGILLOADER_HAS_OIIO)
endif()
add_test(NAME loader_hub_test COMMAND loader_hub_test)

# The hub benchmark (Google Benchmark; use a Release build): cache hits,
# typed-view dispatch and URI resolution, the disk kept out of the timed
# loop. Not a test: it builds through the `benches` target and runs
# through scripts/bench_ledger.py.
add_executable(loader_hub_bench bench/HubBench.cpp)
target_link_libraries(loader_hub_bench PRIVATE SigilLoaderHub
  benchmark::benchmark)
add_dependencies(benches loader_hub_bench)
