# The hub: mounts and resolution, URI selection and retention, the entry cache
# and its poll, network fetch behind the disk cache and the decoder registry —
# split behind private seams.
add_library(SigilIOHub STATIC
  Cache.cpp
  Decoders.cpp
  Mounts.cpp
  Network.cpp
  Retention.cpp
  Selection.cpp
  Fetch.h
  Residency.h
  ${SIGIL_IO_INCLUDE_DIR}/sigilio/hub/Hub.h
  ${SIGIL_IO_INCLUDE_DIR}/sigilio/hub/Network.h
)
target_compile_features(SigilIOHub PUBLIC cxx_std_20)
target_include_directories(SigilIOHub PUBLIC ${SIGIL_IO_INCLUDE_DIR})
target_link_libraries(SigilIOHub PUBLIC SigilIOSource SigilImageDecode
  Boost::container)
find_package(TBB CONFIG REQUIRED)
target_link_libraries(SigilIOHub PRIVATE TBB::tbb)

# libcurl backs the http(s):// URIs (fetch + on-disk cache); pure
# transport, so it stays a private implementation detail.
find_package(CURL CONFIG REQUIRED)
target_link_libraries(SigilIOHub PRIVATE CURL::libcurl)

# The test builds its own PNG fixtures through the image encoder and
# writes its own EXR ones through OIIO directly — the library itself
# encodes nothing.
find_package(OpenImageIO CONFIG QUIET)

add_executable(io_hub_test test/HubTest.cpp)
target_include_directories(io_hub_test PRIVATE ${SIGIL_TEST_SUPPORT_DIR})
target_link_libraries(io_hub_test PRIVATE SigilIOHub SigilImageEncode
  GTest::gtest_main)
if(OpenImageIO_FOUND)
  target_link_libraries(io_hub_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(io_hub_test PRIVATE SIGILIO_HAS_OIIO)
endif()
add_test(NAME io_hub_test COMMAND io_hub_test)

# The hub benchmark (Google Benchmark; use a Release build): cache hits,
# typed-view dispatch and URI resolution, the disk kept out of the timed
# loop. Not a test: it builds through the `benches` target and runs
# through scripts/bench_ledger.py.
add_executable(io_hub_bench bench/HubBench.cpp)
target_link_libraries(io_hub_bench PRIVATE SigilIOHub
  benchmark::benchmark)
add_dependencies(benches io_hub_bench)
