# SigilCoreCache — the caching kernel. Generic over the host: the cache
# policy, the settled-subtree proof that folds one node's declarations and
# its children's verdicts into what the subtree promises, the stability
# release that turns a warmed-up hold into a verdict, and the bake seam
# whose decision — bake now, replay, paint live — is the kernel's while the
# artefact is the host's. Links SigilCoreReconcile for the comparable type
# erasure a seam value takes, and nothing that draws, rasterizes or
# records.
sigil_library(SigilCoreCache
  SOURCES Volatility.cpp
  HEADERS Bake.h Cache.h Policy.h Settle.h Volatility.h
  PUBLIC SigilCoreReconcile)

# Over a fake host, so a link edge that pulled a drawing or rasterizing
# library in would show up as a build failure here.
sigil_test(core_cache_test
  SOURCES test/FakeCacheHost.h test/VolatilityTest.cpp test/SettleTest.cpp
          test/BakeTest.cpp
  LIBRARIES SigilCoreCache)

sigil_bench(core_cache_bench
  SOURCES bench/CacheBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/test
  LIBRARIES SigilCoreCache)
