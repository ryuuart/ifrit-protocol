# The live feature: the reload engine. Qt-free on purpose — it watches a
# source file, rebuilds it into a versioned dylib with the compiler flags
# the build captured, dlopens the result and swaps the running session,
# keeping the previous one alive while a build is broken.
add_library(SigilSketchLive STATIC
  Crash.cpp
  Host.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/live/Crash.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/live/Host.h
)
target_compile_features(SigilSketchLive PUBLIC cxx_std_20)
target_link_libraries(SigilSketchLive PUBLIC SigilSketchCore)
set_target_properties(SigilSketchLive PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_live_test test/HostTest.cpp)
target_link_libraries(sketch_live_test PRIVATE SigilSketchLive
  SigilSketchCanvas GTest::gtest_main)
add_test(NAME sketch_live_test COMMAND sketch_live_test)

add_executable(sketch_live_bench bench/HostBench.cpp)
target_link_libraries(sketch_live_bench PRIVATE SigilSketchLive
  SigilSketchCanvas benchmark::benchmark)
add_dependencies(benches sketch_live_bench)
