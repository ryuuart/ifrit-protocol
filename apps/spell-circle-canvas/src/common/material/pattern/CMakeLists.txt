# The pattern feature: the tile mechanism — one bake, a mapping, an
# explicit reseed — the stock tile generators over it, and the woven
# cloth, whose sett and weave are read per crossing on the CPU and baked
# into a window or a tile through the one rule. Links the texture
# feature (a tile is a repeating texture) and the core compute leaf for
# the seeded hash the speckle scatters with. The stock tiles are painted
# into a Skia canvas, so the Skia executor is linked privately: no
# pattern header names it.
# The two lattice sources, scanlines and the stipple, are programs read
# per pixel; their bodies are the files under shaders/.
set(SIGIL_MATERIAL_PATTERN_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders"
    CACHE INTERNAL "the pattern feature's shader modules")
sigil_library(SigilMaterialPattern
  SOURCES Patterns.cpp Tile.cpp Weave.cpp
  HEADERS Patterns.h Tile.h Weave.h
  PUBLIC SigilMaterialTexture SigilMaterialColor
  PRIVATE SigilCoreCompute SigilMaterialSkia)
sigil_shader_sources(SigilMaterialPattern
  DIR ${SIGIL_MATERIAL_PATTERN_SHADER_DIR} NAMESPACE sigil::material::pattern)

# test/PatternTest.cpp and test/WeaveTest.cpp are compiled into
# material_test, at the library root.

sigil_bench(material_bench
  SOURCES bench/PatternBench.cpp bench/WeaveBench.cpp
  LIBRARIES SigilMaterialPattern SigilMaterialSkia)
