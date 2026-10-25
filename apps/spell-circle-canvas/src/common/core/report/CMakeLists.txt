# SigilCoreReport — the one reporting door: a library's warning said once,
# in one format, to the standard error or to the reporter a host installed.
# Every library that has something to say and no caller to say it to links
# this rather than writing to the standard error itself, so a host hears
# all of them through one hook. Beside it stands the one error a refusing
# door answers, `std::expected<T, core::Error>`, so a caller reads one
# shape whichever library said no. The standard library is the whole of its
# dependencies.
sigil_library(SigilCoreReport
  SOURCES Report.cpp
  HEADERS Error.h Report.h)

# The format and the hook: the line a warning is written as, the standard
# error when no reporter is installed, and an installed reporter hearing
# the warning in its place; and the error a refusing door answers.
sigil_test(core_test
  SOURCES test/ErrorTest.cpp test/ReportTest.cpp
  LIBRARIES SigilCoreReport)
