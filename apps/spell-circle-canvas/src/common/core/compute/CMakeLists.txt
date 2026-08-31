# SigilCoreCompute — the arithmetic several libraries have to agree on to
# the bit: the seeded mixers a jitter draws from, and the folds a cache
# key is accumulated with. The standard library is the whole of its
# dependencies — no glm, no Skia, no kernel — so a shader's CPU twin, a
# point cook, a text cache and a resource store can all reach the same
# body. Header-only, hence an INTERFACE target: there is no archive
# because every function is inline by nature.
add_library(SigilCoreCompute INTERFACE)
target_compile_features(SigilCoreCompute INTERFACE cxx_std_20)
target_include_directories(SigilCoreCompute INTERFACE
  $<BUILD_INTERFACE:${SIGIL_CORE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)

# The geometry path leaf exports itself and links this one, and an
# exported target's dependencies must themselves sit in an export set.
# The header tree installs once, from the library's own CMakeLists.
install(TARGETS SigilCoreCompute EXPORT SigilCoreComputeTargets)
install(EXPORT SigilCoreComputeTargets
  NAMESPACE sigil::core::
  DESTINATION share/sigilcore
)

# The outputs are the contract, so the test pins them: exact words, and
# floats compared as bits. Links only the library it exercises.
add_executable(sigilcore_compute_test
  test/HashTest.cpp
  test/NoiseTest.cpp
)
target_link_libraries(sigilcore_compute_test PRIVATE
  SigilCoreCompute GTest::gtest_main)
add_test(NAME sigilcore_compute_test COMMAND sigilcore_compute_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(sigilcore_compute_bench bench/ComputeBench.cpp)
target_link_libraries(sigilcore_compute_bench PRIVATE
  SigilCoreCompute benchmark::benchmark)
add_dependencies(benches sigilcore_compute_bench)
