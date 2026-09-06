# SigilMeasure — timing, statistics and check reporting, as one feature
# library per directory. `SigilMeasure` is the umbrella: an interface over
# every feature, for a consumer of the whole library.
#
# Every other library measures itself with this one, so it has no
# domain-library dependency at all; its only outside link is the
# Boost.Container header the compact, ordered counter table is built on.
#
# Every public header lives under one include root, include/sigilmeasure/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilmeasure/Measure.h> is the umbrella header over every feature.
sigil_library_root(SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage")

# Dependencies point down this list.
add_subdirectory(stats)
add_subdirectory(time)
add_subdirectory(check)

sigil_library(SigilMeasure
  HEADERS Measure.h
  INTERFACE SigilMeasureStats SigilMeasureTime SigilMeasureCheck)
