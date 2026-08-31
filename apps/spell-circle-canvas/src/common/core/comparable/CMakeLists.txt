# SigilCoreComparable — the two things a value type needs before anything
# can decide it did not change: type erasure that keeps its equality, and
# the field pin that stops a hand-written comparator from quietly
# forgetting a member. The standard library plus Boost.PFR is the whole
# of its dependencies, so any library may link it without pulling a
# kernel, a device or a drawing library behind it. Header-only, hence an
# INTERFACE target: there is no archive because there is nothing to
# compile until a consumer instantiates it.
find_package(boost_pfr CONFIG REQUIRED)
add_library(SigilCoreComparable INTERFACE)
target_compile_features(SigilCoreComparable INTERFACE cxx_std_20)
target_include_directories(SigilCoreComparable INTERFACE
  $<BUILD_INTERFACE:${SIGIL_CORE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
# Fields.h includes <boost/pfr/tuple_size.hpp>, so the include path must
# reach every consumer.
target_link_libraries(SigilCoreComparable INTERFACE Boost::pfr)

# A leaf under the exported path target of another library has to sit in
# an export set of its own, or the export of anything linking it cannot
# name it. The header tree installs once, from the library's own
# CMakeLists.
install(TARGETS SigilCoreComparable EXPORT SigilCoreComparableTargets)
install(EXPORT SigilCoreComparableTargets
  NAMESPACE sigil::core::
  DESTINATION share/sigilcore
)

# Links only the library it exercises, so a link edge that pulled a
# kernel or a motion library in would show up as a build failure here.
add_executable(sigilcore_comparable_test
  test/ErasedTest.cpp
  test/FieldsTest.cpp
)
target_link_libraries(sigilcore_comparable_test PRIVATE
  SigilCoreComparable GTest::gtest_main)
add_test(NAME sigilcore_comparable_test COMMAND sigilcore_comparable_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(sigilcore_comparable_bench bench/ComparableBench.cpp)
target_link_libraries(sigilcore_comparable_bench PRIVATE
  SigilCoreComparable benchmark::benchmark)
add_dependencies(benches sigilcore_comparable_bench)
