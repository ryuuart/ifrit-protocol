# SigilCoreComparable — the two things a value type needs before anything
# can decide it did not change: type erasure that keeps its equality, and
# the field pin that stops a hand-written comparator from quietly
# forgetting a member. The standard library plus Boost.PFR is the whole
# of its dependencies, so any library may link it without pulling a
# kernel, a device or a drawing library behind it. Header-only, hence an
# INTERFACE target: there is no archive because there is nothing to
# compile until a consumer instantiates it. Fields.h includes
# <boost/pfr/tuple_size.hpp>, so the include path must reach every
# consumer.
find_package(boost_pfr CONFIG REQUIRED)
sigil_library(SigilCoreComparable
  INTERFACE Boost::pfr)

# One binary over one file: erasure and the field pin are one subject —
# what a value needs before anything can decide it did not change — and a
# consumer takes both or neither. It links only the library it exercises,
# so a link edge that pulled a kernel or a motion library in would show up
# as a build failure here.
sigil_test(core_comparable_test
  SOURCES test/ErasedTest.cpp
  LIBRARIES SigilCoreComparable)

sigil_bench(core_comparable_bench
  SOURCES bench/ComparableBench.cpp
  LIBRARIES SigilCoreComparable)
