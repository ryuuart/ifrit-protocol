# SigilData — tabular data and the mappings read off it, as one feature
# library per directory. `SigilData` is the umbrella: an interface over
# every feature, for a consumer of the whole library.
#
# Meaning only, and only of a table: what a row is, what a column holds,
# and where a value lands. Where the bytes came from is SigilIO's, and
# what a number says about a population is SigilMeasure's. Nothing here
# draws, and nothing here knows what a colour is.
#
# Every public header lives under one include root, include/sigildata/
# <feature>/, so a feature adds the root and spells its own directory.
# The reference pages: the prose a header's brief stands over, one page
# per entity, named once here and read by both the documentation site and
# the probe below — so a page that is written is a page that is checked.
set(SIGIL_DATA_REFERENCE
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Connection.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Database.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/FlatBuffer.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Json.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Read.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Scale.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Schema.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Table.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/decodeArtNet.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/decodeCsv.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/decodeMidi.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/decodeOsc.md)

sigil_library_root(SigilData
  BRIEF "Tabular data and scales: typed columns, and the one value that maps a domain onto a range"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox ${SIGIL_DATA_REFERENCE})

# Dependencies point down this list.
add_subdirectory(scale)
add_subdirectory(table)
add_subdirectory(decode)
add_subdirectory(values)
add_subdirectory(query)
add_subdirectory(connection)

# SigilDataConnection stands on SigilIOHub, so a consumer of the
# umbrella links the hub as well: a connection is a feed read as values,
# and there is no feed without the hub that opens it.
sigil_library(SigilData
  INTERFACE SigilDataScale SigilDataTable SigilDataDecode SigilDataValues
            SigilDataQuery SigilDataConnection)

# Every API name the prose spells, compiled against the headers that own
# it. What it cannot reach is named with the reason: the value names a
# sketch's own schema generates, the member a generated root supplies to
# a template parameter, and the buffer library's own two pointer types,
# which its headers declare and this library's do not.
sigil_doc_probes(data
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${SIGIL_DATA_REFERENCE}
  EXCLUDE "feed_sky::Sky=a root a sketch's own schema generates, declared by no header of this library"
          "feed_sky::values=the namespace a generated value header puts its values in, declared by no header of this library"
          "sky::Sky=that generated value under the alias an example gives it"
          "sky::Band=that generated value's member table, under the same alias"
          "Root::BinarySchema=what a generated root supplies to the template parameter, not a type declared here"
          "flatbuffers::String=the buffer library's own name, declared in its headers and not in this library's"
          "flatbuffers::Vector=the buffer library's own name, declared in its headers and not in this library's"
  FLOORS 50,18,40,0,0
  LIBRARIES SigilData)
