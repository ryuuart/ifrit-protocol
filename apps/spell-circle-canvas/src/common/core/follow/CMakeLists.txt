# SigilCoreFollow — following: a shared value that counts its own changes,
# the record of which of them one describe read and at which count, and
# the scope a host opens around a describe so every read inside it is
# recorded. The reconciler follows a memo's describe through it, and a
# value library makes its shared values followable through it, so both
# link this leaf and neither links the other. It links the reporting door,
# for the one report a refused write makes, and nothing else of this
# project's.
sigil_library(SigilCoreFollow
  SOURCES Follow.cpp
  HEADERS Follow.h
  PUBLIC SigilCoreReport)

# A followable counter with nothing behind it: reads recorded once each at
# the revision they saw, staleness answered from the counts alone, nested
# scopes, and the refused write reported once.
sigil_test(core_test
  SOURCES test/FollowTest.cpp
  LIBRARIES SigilCoreFollow)
