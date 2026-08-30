# The runtime leaf: whether OpenUSD's file-format plugins are present in
# this process. Every test and benchmark in the library skips through it
# when they are not, so a build with the libraries but without their
# plugin registry reports the fact instead of failing.
add_library(SigilUsdRuntime STATIC
  Runtime.cpp
  ${SIGIL_USD_INCLUDE_DIR}/sigilusd/runtime/Runtime.h
)
target_compile_features(SigilUsdRuntime PUBLIC cxx_std_20)
target_include_directories(SigilUsdRuntime PUBLIC ${SIGIL_USD_INCLUDE_DIR})
target_link_libraries(SigilUsdRuntime PRIVATE usd sdf tf)

# Links only the library it exercises.
add_executable(usd_runtime_test test/RuntimeTest.cpp)
target_link_libraries(usd_runtime_test PRIVATE SigilUsdRuntime GTest::gtest_main)
add_test(NAME usd_runtime_test COMMAND usd_runtime_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(usd_runtime_bench bench/RuntimeBench.cpp)
target_link_libraries(usd_runtime_bench PRIVATE SigilUsdRuntime benchmark::benchmark)
add_dependencies(benches usd_runtime_bench)
