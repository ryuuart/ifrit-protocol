# SigilMeasure — timing, statistics and check reporting. Every other library
# measures itself with this one, so its sole dependency is the Boost.Container
# header used by the compact, ordered counter table.
#
# Every public header lives under one include root, include/sigilmeasure/
# <subject>/, and <sigilmeasure/Measure.h> is the umbrella over all of
# them. One static target: the headers are inline, and the one source
# file holds the table formatting.

set(SIGIL_MEASURE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(SigilMeasure STATIC
  check/Check.cpp
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/Measure.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/check/Check.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/stats/Counters.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/stats/FrameSample.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/stats/Samples.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/time/FrameTimer.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/time/Laps.h
  ${SIGIL_MEASURE_INCLUDE_DIR}/sigilmeasure/time/Stopwatch.h
)
target_compile_features(SigilMeasure PUBLIC cxx_std_20)
target_include_directories(SigilMeasure PUBLIC ${SIGIL_MEASURE_INCLUDE_DIR})
target_link_libraries(SigilMeasure PUBLIC Boost::container)

# Links only the library it exercises, so a link edge that pulled anything
# else in would show up as a build failure here.
add_executable(measure_test test/MeasureTest.cpp)
target_link_libraries(measure_test PRIVATE SigilMeasure GTest::gtest_main)
add_test(NAME measure_test COMMAND measure_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(measure_bench bench/MeasureBench.cpp)
target_link_libraries(measure_bench PRIVATE SigilMeasure benchmark::benchmark)
add_dependencies(benches measure_bench)

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SigilMeasure
  BRIEF "Timing, statistics and check reporting, with compact ordered counter storage"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
