# SigilCoreReconcile — the reconciler kernel. Generic over the host: the
# tree skeleton, the keyed and positional match, the memo with its
# captured environment, the identity prune and the counts are here; what a
# node retains beyond that, and what a patch does to it, is the host's,
# reached through the ReconcileHost operations. Links SigilCoreComparable
# (the erased seam value a description carries and the field pin its
# comparators sit under), SigilMotion (the animatable values, the ticker,
# and the held motions a lane addresses) and SigilMeasure (the published
# counts), and nothing that draws or lays out.
add_library(SigilCoreReconcile STATIC
  Compare.cpp
  Env.cpp
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Compare.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Env.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Erased.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Host.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Lanes.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Memo.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Node.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Phases.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Reconcile.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Reconciler.h
  ${SIGIL_CORE_INCLUDE_DIR}/sigilcore/reconcile/Stats.h
)
target_compile_features(SigilCoreReconcile PUBLIC cxx_std_20)
target_include_directories(SigilCoreReconcile PUBLIC ${SIGIL_CORE_INCLUDE_DIR})
target_link_libraries(SigilCoreReconcile PUBLIC
  SigilCoreComparable SigilMeasure SigilMotionClock SigilMotionValues)

# Links only the library it exercises, over a fake host, so a link edge
# that pulled a drawing or layout library in would show up as a build
# failure here.
add_executable(sigilcore_reconcile_test
  test/FakeHost.h
  test/ReconcilerTest.cpp
  test/EnvTest.cpp
  test/LanesTest.cpp
  test/PhasesTest.cpp
)
target_link_libraries(sigilcore_reconcile_test PRIVATE
  SigilCoreReconcile GTest::gtest_main)
add_test(NAME sigilcore_reconcile_test COMMAND sigilcore_reconcile_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(sigilcore_reconcile_bench bench/ReconcileBench.cpp)
target_include_directories(sigilcore_reconcile_bench PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/test)
target_link_libraries(sigilcore_reconcile_bench PRIVATE
  SigilCoreReconcile benchmark::benchmark)
add_dependencies(benches sigilcore_reconcile_bench)
