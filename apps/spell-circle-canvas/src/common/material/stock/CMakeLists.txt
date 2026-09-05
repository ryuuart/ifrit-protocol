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
sigil_test(material_stock_test
  SOURCES test/StockTest.cpp
  LIBRARIES SigilMaterialStock SigilMaterialField SigilMaterialSdf
            SigilMaterialKit SigilMaterialSkia)
