find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(DuckDB CONFIG REQUIRED)

sigil_library(SigilDataQuery
  SOURCES Database.cpp Common.cpp Sqlite.cpp Duck.cpp Engines.h
  HEADERS Database.h
  PUBLIC SigilDataTable SigilIOSource
  PRIVATE SigilDataDecode unofficial::sqlite3::sqlite3
          $<IF:$<TARGET_EXISTS:duckdb>,duckdb,duckdb_static>)

sigil_test(data_test
  SOURCES test/QueryTest.cpp
  LIBRARIES SigilDataQuery SigilIOHub)
