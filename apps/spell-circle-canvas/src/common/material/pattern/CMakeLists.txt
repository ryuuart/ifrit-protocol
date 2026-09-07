# The pattern feature: the tile mechanism — one bake, a mapping, an
# explicit reseed — the stock tile generators over it, and the woven
# cloth, whose sett and weave are read per crossing on the CPU and baked
# into a window or a tile through the one rule. Links the texture
# feature (a tile is a repeating texture) and the core compute leaf for
# the seeded hash the speckle scatters with.
sigil_library(SigilMaterialPattern
  SOURCES Patterns.cpp Tile.cpp Weave.cpp
  HEADERS Patterns.h Tile.h Weave.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilCoreCompute)

# test/PatternTest.cpp and test/WeaveTest.cpp are compiled into
# material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/PatternBench.cpp bench/WeaveBench.cpp
  LIBRARIES SigilMaterialPattern)
