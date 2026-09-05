# The hub: mounts and resolution, URI selection and retention, the entry cache
# and its poll, network fetch behind the disk cache and the decoder registry —
# split behind private seams — and TextCatalog, the stock value over it that
# a shader catalogue is. oneTBB is the parallel runtime behind the cache and
# libcurl backs the http(s):// URIs (fetch + on-disk cache); both are pure
# implementation and stay private.
find_package(TBB CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
sigil_library(SigilIOHub
  SOURCES Cache.cpp Decoders.cpp Mounts.cpp Network.cpp Retention.cpp
          Selection.cpp Fetch.h Residency.h
  HEADERS Hub.h Network.h TextCatalog.h
  PUBLIC SigilIOSource SigilImageDecode Boost::container
  PRIVATE TBB::tbb CURL::libcurl)

# The test builds its own PNG fixtures through the image encoder and
# writes its own EXR ones through OIIO directly — the library itself
# encodes nothing.
find_package(OpenImageIO CONFIG QUIET)
sigil_test(io_hub_test
  SOURCES test/HubTest.cpp
  LIBRARIES SigilIOHub SigilImageEncode)
if(OpenImageIO_FOUND)
  target_link_libraries(io_hub_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(io_hub_test PRIVATE SIGILIO_HAS_OIIO)
endif()

# Cache hits, typed-view dispatch and URI resolution, the disk kept out
# of the timed loop.
sigil_bench(io_hub_bench
  SOURCES bench/HubBench.cpp
  LIBRARIES SigilIOHub)
