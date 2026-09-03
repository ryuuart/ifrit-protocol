# The graph feature: the order a frame's passes run in, derived from
# what they declared they read and write; the surfaces two resources
# share when their lives do not overlap; the barriers between them; and
# how each pass's selection is realised. It reads declarations and draws
# nothing, so it links the frame feature and nothing else.
add_library(SigilWorldGraph STATIC
  Barriers.cpp
  Claims.cpp
  Order.cpp
  Plan.cpp
  Realise.cpp
  Resources.cpp
  Build.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/graph/Plan.h
)
target_compile_features(SigilWorldGraph PUBLIC cxx_std_20)
target_include_directories(SigilWorldGraph PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldGraph PUBLIC SigilWorldFrame)

add_executable(world_graph_test test/GraphTest.cpp)
target_include_directories(world_graph_test PRIVATE ${SIGIL_WORLD_TEST_DIR})
target_link_libraries(world_graph_test PRIVATE SigilWorldGraph
  GTest::gtest_main)
add_test(NAME world_graph_test COMMAND world_graph_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_graph_bench bench/GraphBench.cpp)
target_link_libraries(world_graph_bench PRIVATE SigilWorldGraph
  benchmark::benchmark)
add_dependencies(benches world_graph_bench)
