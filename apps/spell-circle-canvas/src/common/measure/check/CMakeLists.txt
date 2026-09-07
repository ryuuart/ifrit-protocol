# A claim whose printed verdict is computed from the values it reports,
# and the table a run of claims is printed as. Standard library only: what
# is being checked, and where the sentence is written to, are the caller's.
sigil_library(SigilMeasureCheck
  SOURCES Check.cpp
  HEADERS Check.h)

sigil_test(measure_test
  SOURCES test/CheckTest.cpp
  LIBRARIES SigilMeasureCheck)
