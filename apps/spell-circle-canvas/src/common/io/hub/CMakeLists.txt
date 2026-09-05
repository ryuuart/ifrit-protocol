# The hub: mounts and resolution, URI selection and retention, the entry cache
# and its poll, network fetch behind the disk cache and the decoder registry —
# split behind private seams — and TextCatalog, the stock value over it that
# a shader catalogue is. A preload fans its fetches out through
# SigilCoreSchedule's blocking seam, which is where a call that waits on a
# disk or a server belongs; libcurl backs the http(s):// URIs (fetch +
# on-disk cache). Both are implementation and stay private.
find_package(CURL CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
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
sigil_test(io_test
  SOURCES test/HubTest.cpp
  LIBRARIES SigilIOHub SigilImageEncode SigilCoreSchedule)
# The four EXR cases write their own fixtures through OpenImageIO, so
# they exist only in a build that has it; the label says so.
if(OpenImageIO_FOUND)
  target_link_libraries(io_test PRIVATE OpenImageIO::OpenImageIO)
  target_compile_definitions(io_test PRIVATE SIGILIO_HAS_OIIO)
  sigil_test(io_test SUITES IOOiio LABELS oiio)
endif()

# One case fetches over the network, and skips when there is no route to
# what it asks for. The label is what a run leaves it out by.
sigil_test(io_test
  SUITES IONetwork.LiveFetchThenOfflineRoundTrip LABELS network)

# Cache hits, typed-view dispatch and URI resolution, the disk kept out
# of the timed loop.
sigil_bench(io_bench
  SOURCES bench/HubBench.cpp
  LIBRARIES SigilIOHub)
