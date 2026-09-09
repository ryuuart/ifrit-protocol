# SigilIO — the runtime resource system: mounted res:// URIs,
# caching, metadata probes, hot reload — as one feature library per
# directory, each with its own tests and benchmarks, linking only the
# features beneath it. `SigilIO` is the umbrella: an interface over
# every feature, for a consumer of the whole library.
#
# Access only: the hub is a ByteSource (the source leaf) and every typed
# view is a registered decoder run over the bytes it fetched. All image
# format knowledge lives in SigilImageDecode, whose decoders the hub
# registers by default.
#
# Every public header lives under one include root, include/sigilio/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilio/IO.h> is the umbrella header over every feature.
sigil_library_root(SigilIO
  BRIEF "Runtime resource hub: resources addressed by URI, cached, and reloaded when they change")

# Dependencies point down this list.
add_subdirectory(source)
add_subdirectory(hub)

sigil_library(SigilIO
  HEADERS IO.h
  INTERFACE SigilIOSource SigilIOHub)

# Every API name the prose spells, compiled against the headers that own
# it. SigilImage's decode leaf is scanned and linked because the hub
# links it publicly: the probe and encode names the walkthrough writes
# are that library's, reached through this one.
#
# The names it borrows from ABOVE are exempted instead — the brush
# decoder is registered from SigilDrawBrush, which stands on this
# library — as is the transport target, which is CMake's word and not a
# C++ name at all.
sigil_doc_probes(io
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../image/include/sigilimage
  PRELUDES "<sigilimage/asset/ImageAsset.h>"
           "<sigilimage/encode/Encode.h>"
  EXCLUDE "format::BrushDecoder=SigilDrawBrush stands on this library, so its decoder is named and not probed"
          "brush::Tool=SigilDrawBrush stands on this library, so its brush value is named and not probed"
          "CURL::libcurl=a CMake target name, not a C++ name"
  FLOORS 10,2,2,0,0
  LIBRARIES SigilIO SigilImageDecode)
