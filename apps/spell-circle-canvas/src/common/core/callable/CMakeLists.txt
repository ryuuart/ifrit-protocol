# SigilCoreCallable — the one prefix search in the tree: a callable whose
# parameters are the first few of what a verb offers, called with the ones
# it named, and the type-erased holder a verb stores one in. Every verb
# that offers a caller more than most callers read — a paint program handed
# a canvas and its context, a steppable handed a delta and the clock, a tip
# handed the pen and the dab, an outline handed the box — takes its
# callable through this one header, so the rule about which parameters may
# be dropped is stated and tested once.
#
# The standard library is the whole of its dependencies, and nothing here
# knows what any of those verbs draw or step. Header-only, hence an
# INTERFACE target: there is nothing to compile until a consumer
# instantiates it.
sigil_library(SigilCoreCallable)

# The prefix search is a compile-time answer, so most of the test is
# static_assert; the cases that run prove the CALL — that the parameters a
# callable did not name are still evaluated and dropped, that a holder
# built from each accepted spelling answers the same, and that an empty one
# is false. Links only the library it exercises.
sigil_test(core_test
  SOURCES test/CallableTest.cpp
  LIBRARIES SigilCoreCallable)
