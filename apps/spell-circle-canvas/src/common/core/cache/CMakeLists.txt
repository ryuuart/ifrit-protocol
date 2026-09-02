# SigilCoreCache — the caching kernel. Generic over the host: the cache
# policy, the settled-subtree proof that folds one node's declarations and
# its children's verdicts into what the subtree promises, the stability
# release that turns a warmed-up hold into a verdict, and the bake seam
# whose decision — bake now, replay, paint live — is the kernel's while the
# artefact is the host's. Links SigilCoreReconcile for the comparable type
# erasure a seam value takes, and nothing that draws, rasterizes or
# records.
add_library(SigilCoreCache STATIC
  Volatility.cpp
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/cache/Bake.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/cache/Cache.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/cache/Policy.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/cache/Settle.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/cache/Volatility.h
)
target_compile_features(SigilCoreCache PUBLIC cxx_std_20)
target_include_directories(SigilCoreCache PUBLIC ${SIGIL_CORE_INCLUDE_DIR})
target_link_libraries(SigilCoreCache PUBLIC SigilCoreReconcile)

# Links only the library it exercises, over a fake host, so a link edge
# that pulled a drawing or rasterizing library in would show up as a build
# failure here.
add_executable(sigilcore_cache_test
  test/FakeCacheHost.h
  test/VolatilityTest.cpp
  test/SettleTest.cpp
  test/BakeTest.cpp
)
target_link_libraries(sigilcore_cache_test PRIVATE
  SigilCoreCache GTest::gtest_main)
add_test(NAME sigilcore_cache_test COMMAND sigilcore_cache_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(sigilcore_cache_bench bench/CacheBench.cpp)
target_include_directories(sigilcore_cache_bench PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/test)
target_link_libraries(sigilcore_cache_bench PRIVATE
  SigilCoreCache benchmark::benchmark)
add_dependencies(benches sigilcore_cache_bench)
