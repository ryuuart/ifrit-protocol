# The hub: mounts and resolution, URI selection and retention, the entry cache
# and its poll, network fetch behind the disk cache and the decoder registry —
# split behind private seams — and TextCatalog, the stock value over it that
# a shader catalogue is. A preload fans its fetches out through
# SigilCoreSchedule's blocking seam, which is where a call that waits on a
# disk or a server belongs; libcurl backs the http(s):// URIs (fetch +
# on-disk cache). Both are implementation and stay private.
find_package(CURL CONFIG REQUIRED)
sigil_library(SigilIOHub
  SOURCES Cache.cpp Decoders.cpp Mounts.cpp Network.cpp Retention.cpp
          Selection.cpp Fetch.h Residency.h
  HEADERS Hub.h Network.h TextCatalog.h
  PUBLIC SigilIOSource SigilImageDecode Boost::container
  PRIVATE SigilCoreSchedule CURL::libcurl)

# The test builds its own PNG fixtures through the image encoder and
# writes its own EXR ones through OIIO directly — the library itself
# encodes nothing.
find_package(OpenImageIO CONFIG QUIET)
# SigilCoreSchedule is named here as well as in the library: one case
# preloads more resources than a fan-out runs at once, and it asks that
# seam how many that is rather than guessing a number.
sigil_test(io_hub_test
  SOURCES test/HubTest.cpp
  LIBRARIES SigilIOHub SigilImageEncode SigilCoreSchedule)
# The four EXR cases write their own fixtures through OpenImageIO, so
# they exist only in a build that has it; the label says so.
if(OpenImageIO_FOUND)
  target_link_libraries(io_hub_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(io_hub_test PRIVATE SIGILIO_HAS_OIIO)
else()
  set_tests_properties(io_hub_test PROPERTIES LABELS oiio)
endif()

# One case in that binary fetches over the network. It runs as an entry
# of its own, which is where the `network` label sits, and the entry the
# rest of the cases run under leaves it out, so a default run needs no
# connectivity.
set(_live IONetwork.LiveFetchThenOfflineRoundTrip)
set_tests_properties(io_hub_test PROPERTIES
  ENVIRONMENT "GTEST_FILTER=-${_live}")
add_test(NAME io_hub_network_test
  COMMAND io_hub_test --gtest_filter=${_live})
set_tests_properties(io_hub_network_test PROPERTIES LABELS network)
unset(_live)

# Cache hits, typed-view dispatch and URI resolution, the disk kept out
# of the timed loop.
sigil_bench(io_hub_bench
  SOURCES bench/HubBench.cpp
  LIBRARIES SigilIOHub)
