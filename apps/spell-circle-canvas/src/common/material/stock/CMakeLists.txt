# The stock catalogue: every recipe the library ships, gathered from the
# feature catalogues that own them, and the warm-up over the list. It is
# the one target that links the primitives and the kit together, because
# composing them is what it is for; it links no renderer, so a host still
# chooses the backend its warm-up compiles for.
sigil_library(SigilMaterialStock
  SOURCES Stock.cpp
  HEADERS Stock.h
  PUBLIC SigilMaterialCore
  PRIVATE SigilMaterialField SigilMaterialSdf SigilMaterialKit
          SigilCoreSchedule)

# The test draws nothing; it compiles, so it links the Skia backend for a
# compiler to warm against.
sigil_test(material_test
  SOURCES test/StockTest.cpp
  LIBRARIES SigilMaterialStock SigilMaterialField SigilMaterialSdf
            SigilMaterialKit SigilMaterialSkia)

# The warm-up is the wait before a host's first frame, and it is two
# costs: reading each catalogue's shader files, and compiling what they
# hold. The bench names both, one arm per catalogue beside the whole
# stock, so a recipe added to one family is attributable to that family.
# It links the three catalogues by name for the same reason the test
# does — an arm per family is an arm that calls that family's own list.
sigil_bench(material_bench
  SOURCES bench/StockBench.cpp
  LIBRARIES SigilMaterialStock SigilMaterialField SigilMaterialSdf
            SigilMaterialKit SigilMaterialSkia)
