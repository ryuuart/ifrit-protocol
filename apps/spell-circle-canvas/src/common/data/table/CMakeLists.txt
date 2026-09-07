# The table: named typed columns, the cells as contiguous spans, and the
# row-picking every reshaping is written in terms of. Standard library
# only — where the rows came from is somebody else's business.
sigil_library(SigilDataTable
  SOURCES Table.cpp
  HEADERS Table.h)

sigil_test(data_test
  SOURCES test/TableTest.cpp
  LIBRARIES SigilDataTable)

sigil_bench(data_bench
  SOURCES bench/TableBench.cpp
  LIBRARIES SigilDataTable)
