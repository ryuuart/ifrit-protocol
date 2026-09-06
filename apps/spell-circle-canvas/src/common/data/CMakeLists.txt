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
# <sigildata/Data.h> is the umbrella header over every feature.
sigil_library_root(SigilData
  BRIEF "Tabular data and scales: typed columns, and the one value that maps a domain onto a range")

# Dependencies point down this list.
add_subdirectory(scale)
add_subdirectory(table)

sigil_library(SigilData
  HEADERS Data.h
  INTERFACE SigilDataScale SigilDataTable)
