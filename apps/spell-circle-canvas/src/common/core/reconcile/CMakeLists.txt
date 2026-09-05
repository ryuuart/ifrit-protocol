# SigilCoreReconcile — the reconciler kernel. Generic over the host: the
# tree skeleton, the keyed and positional match, the memo with its
# captured environment, the identity prune and the counts are here; what a
# node retains beyond that, and what a patch does to it, is the host's,
# reached through the ReconcileHost operations. Links SigilCoreComparable
# (the erased seam value a description carries) and SigilMeasure (the
# published counts), and nothing that draws or lays out — and nothing
# that animates either: the lanes a patch retargets and the comparators
# over the values they carry are SigilMotion's, and a host reaches them
# there.
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilCoreReconcile
  SOURCES Env.cpp Reads.cpp
  HEADERS Env.h Erased.h Host.h Memo.h Node.h Phases.h Reads.h Reconcile.h
          Reconciler.h Stats.h
  PUBLIC SigilCoreComparable SigilMeasure Boost::container_hash
         Boost::unordered)

# Over a fake host, so a link edge that pulled a drawing or layout
# library in would show up as a build failure here.
sigil_test(core_test
  SOURCES test/FakeHost.h test/ReconcilerTest.cpp test/EnvTest.cpp
          test/PhasesTest.cpp test/ReadsTest.cpp
  LIBRARIES SigilCoreReconcile)

sigil_bench(core_bench
  SOURCES bench/ReconcileBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/test
  LIBRARIES SigilCoreReconcile)
