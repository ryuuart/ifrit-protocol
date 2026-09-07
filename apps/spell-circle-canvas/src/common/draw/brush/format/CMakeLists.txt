# The brush as a resource: the native directory, and the two importers
# that read a brush authored somewhere else. A brush is one of this
# library's types, so this library is where it is decoded — but only from
# BYTES: SigilIOSource is the vocabulary a decoder speaks — and the
# archive source the two zip formats are read through, because one file
# with files inside it is resource access and not brush knowledge —
# SigilImage turns the artwork inside a brush into an image, and neither
# this target nor anything under it opens a file. simdjson parses the
# native description; it is private plumbing that reaches no public
# header.
find_package(simdjson CONFIG REQUIRED)
sigil_library(SigilDrawBrushFormat
  SOURCES Import.cpp Import.h Native.cpp Photoshop.cpp Procreate.cpp
  HEADERS Load.h Photoshop.h Procreate.h
  PUBLIC SigilDrawBrush SigilIOSource
  PRIVATE SigilImageDecode simdjson::simdjson)

# The cases build every fixture in memory — a generated tip encoded to
# PNG, a zip written by hand, an .abr written by hand — so the binary
# needs the encoder half of SigilImage that the library itself does not.
sigil_test(draw_test
  SOURCES test/FormatTest.cpp
  LIBRARIES SigilDrawBrushFormat SigilIOHub SigilImageEncode SigilWeavePorts)
