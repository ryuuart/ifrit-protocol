# SigilLoader — the runtime resource system: mounted res:// URIs,
# caching, metadata probes, hot reload — as one feature library per
# directory, each with its own tests and benchmarks, linking only the
# features beneath it. `SigilLoader` is the umbrella: an interface over
# every feature, for a consumer of the whole library.
#
# Access only: the hub is a ByteSource (the source leaf) and every typed
# view is a registered decoder run over the bytes it fetched. All image
# format knowledge lives in SigilImageDecode, whose decoders the hub
# registers by default.
#
# Every public header lives under one include root, include/sigilloader/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilloader/Loader.h> is the umbrella header over every feature.

set(SIGIL_LOADER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point down this list.
add_subdirectory(source)
add_subdirectory(hub)

add_library(SigilLoader INTERFACE
  ${SIGIL_LOADER_INCLUDE_DIR}/sigilloader/Loader.h
)
target_link_libraries(SigilLoader INTERFACE SigilLoaderSource SigilLoaderHub)

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
