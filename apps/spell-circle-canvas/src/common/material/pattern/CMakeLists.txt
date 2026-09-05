# The pattern feature: the tile mechanism — one bake, a mapping, an
# explicit reseed — and the stock tile generators over it. Links the
# texture feature (a tile is a repeating texture) and the core compute leaf
# for the seeded hash the speckle scatters with.
sigil_library(SigilMaterialPattern
  SOURCES Patterns.cpp Tile.cpp
  HEADERS Patterns.h Tile.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilCoreCompute)

# test/PatternTest.cpp is compiled into material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/PatternBench.cpp
  LIBRARIES SigilMaterialPattern)
