# SigilIO — the runtime resource system: mounted res:// URIs,
# caching, metadata probes, hot reload — as one feature library per
# directory, linking only the features beneath it. Tests and benchmarks
# collect into one binary each. `SigilIO` is the resource umbrella; network
# transport and native publication are separately linked features.
#
# Access only: the hub is a ByteSource (the source leaf) and every typed
# view is a registered decoder run over the bytes it fetched. All image
# format knowledge lives in SigilImageDecode, whose decoders the hub
# registers by default.
#
# Every public header lives under one include root, include/sigilio/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilio/IO.h> is the umbrella header for source and hub access.
sigil_library_root(SigilIO
  BRIEF "Runtime resource access, byte transports and native frame sharing"
  DOCS publish/README.md)

# Dependencies point down this list.
add_subdirectory(source)
add_subdirectory(hub)
# Stands on the hub — it teaches one a scheme — and is linked by a
# consumer that opens network feeds and by no other.
add_subdirectory(transport)
# Native frame sharing is optional and carries no hub or byte transport.
add_subdirectory(publish)

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
# C++ name at all. The transport feature is linked because the prose
# spells its registration, which the umbrella does not carry.
sigil_doc_probes(io
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/publish/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../image/include/sigilimage
  PRELUDES "<sigilimage/asset/ImageAsset.h>"
           "<sigilimage/encode/Encode.h>"
  EXCLUDE "format::BrushDecoder=SigilDrawBrush stands on this library, so its decoder is named and not probed"
          "brush::Tool=SigilDrawBrush stands on this library, so its brush value is named and not probed"
          "CURL::libcurl=a CMake target name, not a C++ name"
  FLOORS 13,4,9,6,0
  LIBRARIES SigilIO SigilIOTransport SigilIOPublish SigilImageDecode)
