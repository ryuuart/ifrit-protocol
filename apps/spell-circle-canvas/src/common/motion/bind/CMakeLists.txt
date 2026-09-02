# The bind leaf: bind(), the Bound chain builder, the BoundFloat
# evaluator it fills, the value-noise field behind wiggle(), and the two
# comparators an identity prune reads a curve and a shaped binding
# through — here because the record this feature owns is the lowest thing
# in the library that carries a curve.
#
# Choreograph, cmath and integers, plus SigilCoreComparable for the field
# pin the shaped-binding comparator sits under: an edge OUT of this
# library, to a header-only leaf over the standard library and Boost.PFR.
add_library(SigilMotionBind STATIC
  Bound.cpp
  BoundFloat.cpp
  WiggleNoise.cpp
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/bind/Bind.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/bind/Bound.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/bind/BoundFloat.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/bind/WiggleNoise.h
)
target_compile_features(SigilMotionBind PUBLIC cxx_std_20)
target_include_directories(SigilMotionBind PUBLIC ${SIGIL_MOTION_INCLUDE_DIR})
target_link_libraries(SigilMotionBind PUBLIC
  SigilCoreComparable choreograph::choreograph)

# Links only the library it exercises, so a link edge that pulled a
# drawing library in would show up as a build failure here.
add_executable(motion_bind_test test/BindTest.cpp)
target_link_libraries(motion_bind_test PRIVATE SigilMotionBind GTest::gtest_main)
add_test(NAME motion_bind_test COMMAND motion_bind_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(motion_bind_bench bench/BindBench.cpp)
target_link_libraries(motion_bind_bench PRIVATE SigilMotionBind
  benchmark::benchmark)
add_dependencies(benches motion_bind_bench)
