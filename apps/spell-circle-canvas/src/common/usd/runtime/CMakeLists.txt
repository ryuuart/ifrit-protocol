# The runtime leaf: whether OpenUSD's file-format plugins are present in
# this process. Every test and benchmark in the library skips through it
# when they are not, so a build with the libraries but without their
# plugin registry reports the fact instead of failing.
sigil_library(SigilUsdRuntime
  SOURCES Runtime.cpp
  HEADERS Runtime.h
  PRIVATE usd sdf tf)

# The probe's own claim — it answers, and answers the same way twice —
# is asserted in usd_read_test, where a case that skips on it stands
# beside it. This leaf carries no test binary of its own.
sigil_bench(usd_runtime_bench
  SOURCES bench/RuntimeBench.cpp
  LIBRARIES SigilUsdRuntime)
