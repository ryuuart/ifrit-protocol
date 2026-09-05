# The brush as a resource: the native directory, and the two importers
# that read a brush authored somewhere else. A brush is one of this
# library's types, so this library is where it is decoded — but only from
# BYTES: SigilIOSource is the vocabulary a decoder speaks, SigilImage
# turns the artwork inside a brush into an image, and neither this target
# nor anything under it opens a file. simdjson parses the native
# description; minizip-ng opens the two archive formats. Both are private
# plumbing that reaches no public header.
find_package(minizip-ng CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
sigil_library(SigilDrawBrushFormat PIC
  SOURCES Images.cpp Images.h Native.cpp Photoshop.cpp Procreate.cpp
          Zip.cpp Zip.h
  HEADERS Load.h Photoshop.h Procreate.h
  PUBLIC SigilDrawBrush SigilIOSource
  PRIVATE SigilImageDecode simdjson::simdjson MINIZIP::minizip-ng)

# The cases build every fixture in memory — a generated tip encoded to
# PNG, a zip written by hand, an .abr written by hand — so the binary
# needs the encoder half of SigilImage that the library itself does not.
sigil_test(draw_brush_format_test
  SOURCES test/FormatTest.cpp
  LIBRARIES SigilDrawBrushFormat SigilIOHub SigilImageEncode SigilWeavePorts)
